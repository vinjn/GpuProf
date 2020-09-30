#include "etw_prof.h"
#include "../3rdparty/PresentMon/PresentData/TraceSession.hpp"

#pragma comment(lib, "Tdh")
#include <VersionHelpers.h>

namespace {

    TraceSession gSession;
    PMTraceConsumer* gPMConsumer = nullptr;
    auto mSessionName = "GpuProf";
    std::thread sConsumerThread;
    std::thread sProcessThread;
    bool gQuit = false;


    // When we collect realtime ETW events, we don't receive the events in real
    // time but rather sometime after they occur.  Since the user might be toggling
    // recording based on realtime cues (e.g., watching the target application) we
    // maintain a history of realtime record toggle events from the user.  When we
    // consider recording an event, we can look back and see what the recording
    // state was at the time the event actually occurred.
    //
    // gRecordingToggleHistory is a vector of QueryPerformanceCounter() values at
    // times when the recording state changed, and gIsRecording is the recording
    // state at the current time.
    //
    // CRITICAL_SECTION used as this is expected to have low contention (e.g., *no*
    // contention when capturing from ETL).

    CRITICAL_SECTION gRecordingToggleCS;
    std::vector<uint64_t> gRecordingToggleHistory;
    bool gIsRecording = false;
}


static void Consume(TRACEHANDLE traceHandle)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // You must call OpenTrace() prior to calling this function
    //
    // ProcessTrace() blocks the calling thread until it
    //     1) delivers all events in a trace log file, or
    //     2) the BufferCallback function returns FALSE, or
    //     3) you call CloseTrace(), or
    //     4) the controller stops the trace session.
    //
    // There may be a several second delay before the function returns.
    //
    // ProcessTrace() is supposed to return ERROR_CANCELLED if BufferCallback
    // (EtwThreadsShouldQuit) returns FALSE; and ERROR_SUCCESS if the trace
    // completes (parses the entire ETL, fills the maximum file size, or is
    // explicitly closed).
    //
    // However, it seems to always return ERROR_SUCCESS.

    auto status = ProcessTrace(&traceHandle, 1, NULL, NULL);
    (void)status;
}

void StartConsumerThread(TRACEHANDLE traceHandle)
{
    sConsumerThread = std::thread(Consume, traceHandle);
}

void WaitForConsumerThreadToExit()
{
    if (sConsumerThread.joinable()) {
        sConsumerThread.join();
    }
}

bool etw_setup()
{
    auto simple = true;
    auto expectFilteredEvents = true;

    // Create consumers
    gPMConsumer = new PMTraceConsumer(expectFilteredEvents, simple);

    // Start the session;
    // If a session with this same name is already running, we either exit or
    // stop it and start a new session.  This is useful if a previous process
    // failed to properly shut down the session for some reason.
    auto status = gSession.Start(gPMConsumer, nullptr, nullptr, mSessionName);

    if (status == ERROR_ALREADY_EXISTS) {
        if (true /*args.mStopExistingSession*/) {
            fprintf(stderr,
                "warning: a trace session named \"%s\" is already running and it will be stopped.\n"
                "         Use -session_name with a different name to start a new session.\n",
                mSessionName);
        }
        else {
            fprintf(stderr,
                "error: a trace session named \"%s\" is already running. Use -stop_existing_session\n"
                "       to stop the existing session, or use -session_name with a different name to\n"
                "       start a new session.\n",
                mSessionName);
            delete gPMConsumer;
            return false;
        }

        status = TraceSession::StopNamedSession(mSessionName);
        if (status == ERROR_SUCCESS) {
            status = gSession.Start(gPMConsumer, nullptr, nullptr, mSessionName);
        }
    }

    // Report error if we failed to start a new session
    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "error: failed to start trace session");
        switch (status) {
        case ERROR_FILE_NOT_FOUND:    fprintf(stderr, " (file not found)"); break;
        case ERROR_PATH_NOT_FOUND:    fprintf(stderr, " (path not found)"); break;
        case ERROR_BAD_PATHNAME:      fprintf(stderr, " (invalid --session_name)"); break;
        case ERROR_ACCESS_DENIED:     fprintf(stderr, " (access denied)"); break;
        case ERROR_FILE_CORRUPT:      fprintf(stderr, " (invalid --etl_file)"); break;
        default:                      fprintf(stderr, " (error=%u)", status); break;
        }
        fprintf(stderr, ".\n");

        delete gPMConsumer;
        gPMConsumer = nullptr;
        return false;
    }

    // -------------------------------------------------------------------------
    // Start the consumer and output threads
    StartConsumerThread(gSession.mTraceHandle);

    return true;
}

void etw_quit()
{
    // Stop the trace session.
    gSession.Stop();

    // Wait for the consumer and output threads to end (which are using the
    // consumers).
    WaitForConsumerThreadToExit();

    // Destruct the consumers
    delete gPMConsumer;
    gPMConsumer = nullptr;
}

#if 0

void CheckLostReports(ULONG* eventsLost, ULONG* buffersLost)
{
    auto status = gSession.CheckLostReports(eventsLost, buffersLost);
    (void)status;
}

void DequeueAnalyzedInfo(
    std::vector<ProcessEvent>* processEvents,
    std::vector<std::shared_ptr<PresentEvent>>* presentEvents)
{
    gPMConsumer->DequeueProcessEvents(*processEvents);
    gPMConsumer->DequeuePresentEvents(*presentEvents);
}

double QpcDeltaToSeconds(uint64_t qpcDelta)
{
    return (double)qpcDelta / gSession.mQpcFrequency.QuadPart;
}

uint64_t SecondsDeltaToQpc(double secondsDelta)
{
    return (uint64_t)(secondsDelta * gSession.mQpcFrequency.QuadPart);
}

double QpcToSeconds(uint64_t qpc)
{
    return QpcDeltaToSeconds(qpc - gSession.mStartQpc.QuadPart);
}



void SetOutputRecordingState(bool record)
{
    auto const& args = GetCommandLineArgs();

    if (gIsRecording == record) {
        return;
    }

    // When capturing from an ETL file, just use the current recording state.
    // It's not clear how best to map realtime to ETL QPC time, and there
    // aren't any realtime cues in this case.
    if (args.mEtlFileName != nullptr) {
        EnterCriticalSection(&gRecordingToggleCS);
        gIsRecording = record;
        LeaveCriticalSection(&gRecordingToggleCS);
        return;
    }

    uint64_t qpc = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&qpc);

    EnterCriticalSection(&gRecordingToggleCS);
    gRecordingToggleHistory.emplace_back(qpc);
    gIsRecording = record;
    LeaveCriticalSection(&gRecordingToggleCS);
}

static bool CopyRecordingToggleHistory(std::vector<uint64_t>* recordingToggleHistory)
{
    EnterCriticalSection(&gRecordingToggleCS);
    recordingToggleHistory->assign(gRecordingToggleHistory.begin(), gRecordingToggleHistory.end());
    auto isRecording = gIsRecording;
    LeaveCriticalSection(&gRecordingToggleCS);

    auto recording = recordingToggleHistory->size() + (isRecording ? 1 : 0);
    return (recording & 1) == 1;
}

// Remove recording toggle events that we've processed.
static void UpdateRecordingToggles(size_t nextIndex)
{
    if (nextIndex > 0) {
        EnterCriticalSection(&gRecordingToggleCS);
        gRecordingToggleHistory.erase(gRecordingToggleHistory.begin(), gRecordingToggleHistory.begin() + nextIndex);
        LeaveCriticalSection(&gRecordingToggleCS);
    }
}

// Processes are handled differently when running in realtime collection vs.
// ETL collection.  When reading an ETL, we receive NT_PROCESS events whenever
// a process is created or exits which we use to update the active processes.
//
// When collecting events in realtime, we update the active processes whenever
// we notice an event with a new process id.  If it's a target process, we
// obtain a handle to the process, and periodically check it to see if it has
// exited.

static std::unordered_map<uint32_t, ProcessInfo> gProcesses;
static uint32_t gTargetProcessCount = 0;

static bool IsTargetProcess(uint32_t processId, std::string const& processName)
{
    auto const& args = GetCommandLineArgs();

    // -exclude
    for (auto excludeProcessName : args.mExcludeProcessNames) {
        if (_stricmp(excludeProcessName, processName.c_str()) == 0) {
            return false;
        }
    }

    // -capture_all
    if (args.mTargetPid == 0 && args.mTargetProcessNames.empty()) {
        return true;
    }

    // -process_id
    if (args.mTargetPid != 0 && args.mTargetPid == processId) {
        return true;
    }

    // -process_name
    for (auto targetProcessName : args.mTargetProcessNames) {
        if (_stricmp(targetProcessName, processName.c_str()) == 0) {
            return true;
        }
    }

    return false;
}

static void InitProcessInfo(ProcessInfo* processInfo, uint32_t processId, HANDLE handle, std::string const& processName)
{
    auto target = IsTargetProcess(processId, processName);

    processInfo->mHandle = handle;
    processInfo->mModuleName = processName;
    processInfo->mOutputCsv.mFile = nullptr;
    processInfo->mOutputCsv.mWmrFile = nullptr;
    processInfo->mTargetProcess = target;

    if (target) {
        gTargetProcessCount += 1;
    }
}

static ProcessInfo* GetProcessInfo(uint32_t processId)
{
    auto result = gProcesses.emplace(processId, ProcessInfo());
    auto processInfo = &result.first->second;
    auto newProcess = result.second;

    if (newProcess) {
        // In ETL capture, we should have gotten an NTProcessEvent for this
        // process updated via UpdateNTProcesses(), so this path should only
        // happen in realtime capture.
        auto const& args = GetCommandLineArgs();
        HANDLE handle = NULL;
        char const* processName = "<error>";
        if (args.mEtlFileName == nullptr) {
            char path[MAX_PATH];
            DWORD numChars = sizeof(path);
            handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
            if (QueryFullProcessImageNameA(handle, 0, path, &numChars)) {
                processName = PathFindFileNameA(path);
            }
        }

        InitProcessInfo(processInfo, processId, handle, processName);
    }

    return processInfo;
}

// Check if any realtime processes terminated and add them to the terminated
// list.
//
// We assume that the process terminated now, which is wrong but conservative
// and functionally ok because no other process should start with the same PID
// as long as we're still holding a handle to it.
static void CheckForTerminatedRealtimeProcesses(std::vector<std::pair<uint32_t, uint64_t>>* terminatedProcesses)
{
    for (auto& pair : gProcesses) {
        auto processId = pair.first;
        auto processInfo = &pair.second;

        DWORD exitCode = 0;
        if (processInfo->mHandle != NULL && GetExitCodeProcess(processInfo->mHandle, &exitCode) && exitCode != STILL_ACTIVE) {
            uint64_t qpc = 0;
            QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
            terminatedProcesses->emplace_back(processId, qpc);
            CloseHandle(processInfo->mHandle);
            processInfo->mHandle = NULL;
        }
    }
}

static void HandleTerminatedProcess(uint32_t processId)
{
    auto const& args = GetCommandLineArgs();

    auto iter = gProcesses.find(processId);
    if (iter == gProcesses.end()) {
        return; // shouldn't happen.
    }

    auto processInfo = &iter->second;
    if (processInfo->mTargetProcess) {
        // Close this process' CSV.
        CloseOutputCsv(processInfo);

        // Quit if this is the last process tracked for -terminate_on_proc_exit.
        gTargetProcessCount -= 1;
        if (args.mTerminateOnProcExit && gTargetProcessCount == 0) {
            ExitMainThread();
        }
    }

    gProcesses.erase(iter);
}

static void UpdateProcesses(std::vector<ProcessEvent> const& processEvents, std::vector<std::pair<uint32_t, uint64_t>>* terminatedProcesses)
{
    for (auto const& processEvent : processEvents) {
        if (processEvent.IsStartEvent) {
            // This event is a new process starting, the pid should not already be
            // in gProcesses.
            auto result = gProcesses.emplace(processEvent.ProcessId, ProcessInfo());
            auto processInfo = &result.first->second;
            auto newProcess = result.second;
            if (newProcess) {
                InitProcessInfo(processInfo, processEvent.ProcessId, NULL, processEvent.ImageFileName);
            }
        }
        else {
            // Note any process termination in terminatedProcess, to be handled
            // once the present event stream catches up to the termination time.
            terminatedProcesses->emplace_back(processEvent.ProcessId, processEvent.QpcTime);
        }
    }
}

static void AddPresents(std::vector<std::shared_ptr<PresentEvent>> const& presentEvents, size_t* presentEventIndex,
    bool recording, bool checkStopQpc, uint64_t stopQpc, bool* hitStopQpc)
{
    auto i = *presentEventIndex;
    for (auto n = presentEvents.size(); i < n; ++i) {
        auto presentEvent = presentEvents[i];

        // Stop processing events if we hit the next stop time.
        if (checkStopQpc && presentEvent->QpcTime >= stopQpc) {
            *hitStopQpc = true;
            break;
        }

        // Look up the swapchain this present belongs to.
        auto processInfo = GetProcessInfo(presentEvent->ProcessId);
        if (!processInfo->mTargetProcess) {
            continue;
        }

        auto result = processInfo->mSwapChain.emplace(presentEvent->SwapChainAddress, SwapChainData());
        auto chain = &result.first->second;
        if (result.second) {
            chain->mPresentHistoryCount = 0;
            chain->mNextPresentIndex = 1; // Start at 1 so that mLastDisplayedPresentIndex starts out invalid.
            chain->mLastDisplayedPresentIndex = 0;
        }

        // Output CSV row if recording (need to do this before updating chain).
        if (recording) {
            UpdateCsv(processInfo, *chain, *presentEvent);
        }

        // Add the present to the swapchain history.
        chain->mPresentHistory[chain->mNextPresentIndex % SwapChainData::PRESENT_HISTORY_MAX_COUNT] = presentEvent;

        if (presentEvent->FinalState == PresentResult::Presented) {
            chain->mLastDisplayedPresentIndex = chain->mNextPresentIndex;
        }
        else if (chain->mLastDisplayedPresentIndex == chain->mNextPresentIndex) {
            chain->mLastDisplayedPresentIndex = 0;
        }

        chain->mNextPresentIndex += 1;
        if (chain->mPresentHistoryCount < SwapChainData::PRESENT_HISTORY_MAX_COUNT) {
            chain->mPresentHistoryCount += 1;
        }
    }

    *presentEventIndex = i;
}

static void AddPresents(LateStageReprojectionData* lsrData,
    std::vector<std::shared_ptr<LateStageReprojectionEvent>> const& presentEvents, size_t* presentEventIndex,
    bool recording, bool checkStopQpc, uint64_t stopQpc, bool* hitStopQpc)
{
    auto const& args = GetCommandLineArgs();

    auto i = *presentEventIndex;
    for (auto n = presentEvents.size(); i < n; ++i) {
        auto presentEvent = presentEvents[i];

        // Stop processing events if we hit the next stop time.
        if (checkStopQpc && presentEvent->QpcTime >= stopQpc) {
            *hitStopQpc = true;
            break;
        }

        const uint32_t appProcessId = presentEvent->GetAppProcessId();
        auto processInfo = GetProcessInfo(appProcessId);
        if (!processInfo->mTargetProcess) {
            continue;
        }

        if ((args.mVerbosity > Verbosity::Simple) && (appProcessId == 0)) {
            continue; // Incomplete event data
        }

        lsrData->AddLateStageReprojection(*presentEvent);

        if (recording) {
            UpdateLsrCsv(*lsrData, processInfo, *presentEvent);
        }

        lsrData->UpdateLateStageReprojectionInfo();
    }

    *presentEventIndex = i;
}

// Limit the present history stored in SwapChainData to 2 seconds.
static void PruneHistory(
    std::vector<ProcessEvent> const& processEvents,
    std::vector<std::shared_ptr<PresentEvent>> const& presentEvents,
    std::vector<std::shared_ptr<LateStageReprojectionEvent>> const& lsrEvents)
{
    assert(processEvents.size() + presentEvents.size() + lsrEvents.size() > 0);

    auto latestQpc = max(max(
        processEvents.empty() ? 0ull : processEvents.back().QpcTime,
        presentEvents.empty() ? 0ull : presentEvents.back()->QpcTime),
        lsrEvents.empty() ? 0ull : lsrEvents.back()->QpcTime);

    auto minQpc = latestQpc - SecondsDeltaToQpc(2.0);

    for (auto& pair : gProcesses) {
        auto processInfo = &pair.second;
        for (auto& pair2 : processInfo->mSwapChain) {
            auto swapChain = &pair2.second;

            auto count = swapChain->mPresentHistoryCount;
            for (; count > 0; --count) {
                auto index = swapChain->mNextPresentIndex - count;
                auto const& presentEvent = swapChain->mPresentHistory[index % SwapChainData::PRESENT_HISTORY_MAX_COUNT];
                if (presentEvent->QpcTime >= minQpc) {
                    break;
                }
                if (index == swapChain->mLastDisplayedPresentIndex) {
                    swapChain->mLastDisplayedPresentIndex = 0;
                }
            }

            swapChain->mPresentHistoryCount = count;
        }
    }
}

static void ProcessEvents(
    LateStageReprojectionData* lsrData,
    std::vector<ProcessEvent>* processEvents,
    std::vector<std::shared_ptr<PresentEvent>>* presentEvents,
    std::vector<std::shared_ptr<LateStageReprojectionEvent>>* lsrEvents,
    std::vector<uint64_t>* recordingToggleHistory,
    std::vector<std::pair<uint32_t, uint64_t>>* terminatedProcesses)
{
    auto const& args = GetCommandLineArgs();

    // Copy any analyzed information from ConsumerThread and early-out if there
    // isn't any.
    DequeueAnalyzedInfo(processEvents, presentEvents, lsrEvents);
    if (processEvents->empty() && presentEvents->empty() && lsrEvents->empty()) {
        return;
    }

    // Copy the record range history form the MainThread.
    auto recording = CopyRecordingToggleHistory(recordingToggleHistory);

    // Handle Process events; created processes are added to gProcesses and
    // terminated processes are added to terminatedProcesses.
    //
    // Handling of terminated processes need to be deferred until we observe a
    // present event that started after the termination time.  This is because
    // while a present must start before termination, it can complete after
    // termination.
    //
    // We don't have to worry about the recording toggles here because
    // NTProcess events are only captured when parsing ETL files and we don't
    // use recording toggle history for ETL files.
    UpdateProcesses(*processEvents, terminatedProcesses);

    // Next, iterate through the recording toggles (if any)...
    size_t presentEventIndex = 0;
    size_t lsrEventIndex = 0;
    size_t recordingToggleIndex = 0;
    size_t terminatedProcessIndex = 0;
    for (;;) {
        auto checkRecordingToggle = recordingToggleIndex < recordingToggleHistory->size();
        auto nextRecordingToggleQpc = checkRecordingToggle ? (*recordingToggleHistory)[recordingToggleIndex] : 0ull;
        auto hitNextRecordingToggle = false;

        // First iterate through the terminated process history up until the
        // next recording toggle.  If we hit a present that started after the
        // termination, we can handle the process termination and continue.
        // Otherwise, we're done handling all the presents and any outstanding
        // terminations will have to wait for the next batch of events.
        for (; terminatedProcessIndex < terminatedProcesses->size(); ++terminatedProcessIndex) {
            auto const& pair = (*terminatedProcesses)[terminatedProcessIndex];
            auto terminatedProcessId = pair.first;
            auto terminatedProcessQpc = pair.second;

            if (checkRecordingToggle && nextRecordingToggleQpc < terminatedProcessQpc) {
                break;
            }

            auto hitTerminatedProcess = false;
            AddPresents(*presentEvents, &presentEventIndex, recording, true, terminatedProcessQpc, &hitTerminatedProcess);
            AddPresents(lsrData, *lsrEvents, &lsrEventIndex, recording, true, terminatedProcessQpc, &hitTerminatedProcess);
            if (!hitTerminatedProcess) {
                goto done;
            }
            HandleTerminatedProcess(terminatedProcessId);
        }

        // Process present events up until the next recording toggle.  If we
        // reached the toggle, handle it and continue.  Otherwise, we're done
        // handling all the presents and any outstanding toggles will have to
        // wait for next batch of events.
        AddPresents(*presentEvents, &presentEventIndex, recording, checkRecordingToggle, nextRecordingToggleQpc, &hitNextRecordingToggle);
        AddPresents(lsrData, *lsrEvents, &lsrEventIndex, recording, checkRecordingToggle, nextRecordingToggleQpc, &hitNextRecordingToggle);
        if (!hitNextRecordingToggle) {
            break;
        }

        // Toggle recording.
        recordingToggleIndex += 1;
        recording = !recording;
        if (!recording) {
            IncrementRecordingCount();
            CloseOutputCsv(nullptr);
            for (auto& pair : gProcesses) {
                CloseOutputCsv(&pair.second);
            }
        }
    }

done:

    // Limit the present history stored in SwapChainData to 2 seconds, so that
    // processes that stop presenting are removed from the console display.
    // This only applies to ConsoleOutput::Full, otherwise it's ok to just
    // leave the older presents in the history buffer since they aren't used
    // for anything.
    if (args.mConsoleOutputType == ConsoleOutput::Full) {
        PruneHistory(*processEvents, *presentEvents, *lsrEvents);
    }

    // Clear events processed.
    processEvents->clear();
    presentEvents->clear();
    lsrEvents->clear();
    recordingToggleHistory->clear();

    // Finished processing all events.  Erase the recording toggles and
    // terminated processes that we also handled now.
    UpdateRecordingToggles(recordingToggleIndex);
    if (terminatedProcessIndex > 0) {
        terminatedProcesses->erase(terminatedProcesses->begin(), terminatedProcesses->begin() + terminatedProcessIndex);
    }

    if (DebugDone()) {
        ExitMainThread();
    }
}

void Output()
{
#if !DEBUG_VERBOSE
    auto const& args = GetCommandLineArgs();
#endif

    // Structures to track processes and statistics from recorded events.
    LateStageReprojectionData lsrData;
    std::vector<ProcessEvent> processEvents;
    std::vector<std::shared_ptr<PresentEvent>> presentEvents;
    std::vector<std::shared_ptr<LateStageReprojectionEvent>> lsrEvents;
    std::vector<uint64_t> recordingToggleHistory;
    std::vector<std::pair<uint32_t, uint64_t>> terminatedProcesses;
    processEvents.reserve(128);
    presentEvents.reserve(4096);
    lsrEvents.reserve(4096);
    recordingToggleHistory.reserve(16);
    terminatedProcesses.reserve(16);

    for (;;) {
        // Read gQuit here, but then check it after processing queued events.
        // This ensures that we call DequeueAnalyzedInfo() at least once after
        // events have stopped being collected so that all events are included.
        auto quit = gQuit;

        // Copy and process all the collected events, and update the various
        // tracking and statistics data structures.
        ProcessEvents(&lsrData, &processEvents, &presentEvents, &lsrEvents, &recordingToggleHistory, &terminatedProcesses);

        // Display information to console if requested.  If debug build and
        // simple console, print a heartbeat if recording.
        //
        // gIsRecording is the real timeline recording state.  Because we're
        // just reading it without correlation to gRecordingToggleHistory, we
        // don't need the critical section.
#if !DEBUG_VERBOSE
        auto realtimeRecording = gIsRecording;
        switch (args.mConsoleOutputType) {
        case ConsoleOutput::None:
            break;
        case ConsoleOutput::Simple:
#if _DEBUG
            if (realtimeRecording) {
                printf(".");
            }
#endif
            break;
        case ConsoleOutput::Full:
            for (auto const& pair : gProcesses) {
                UpdateConsole(pair.first, pair.second);
            }
            UpdateConsole(gProcesses, lsrData);

            if (realtimeRecording) {
                ConsolePrintLn("** RECORDING **");
            }
            CommitConsole();
            break;
        }
#endif

        // Everything is processed and output out at this point, so if we're
        // quiting we don't need to update the rest.
        if (quit) {
            break;
        }

        // Update tracking information.
        CheckForTerminatedRealtimeProcesses(&terminatedProcesses);

        // Sleep to reduce overhead.
        ::Sleep(1);
    }

    // Output warning if events were lost.
    ULONG eventsLost = 0;
    ULONG buffersLost = 0;
    CheckLostReports(&eventsLost, &buffersLost);
    if (buffersLost > 0) {
        fprintf(stderr, "warning: %lu ETW buffers were lost.\n", buffersLost);
    }
    if (eventsLost > 0) {
        fprintf(stderr, "warning: %lu ETW events were lost.\n", eventsLost);
    }

    // Close all CSV and process handles
    for (auto& pair : gProcesses) {
        auto processInfo = &pair.second;
        if (processInfo->mHandle != NULL) {
            CloseHandle(processInfo->mHandle);
        }
        CloseOutputCsv(processInfo);
    }
    gProcesses.clear();
}

void StartOutputThread()
{
    InitializeCriticalSection(&gRecordingToggleCS);

    sProcessThread = std::thread(Output);
}

void StopOutputThread()
{
    if (sProcessThread.joinable()) {
        gQuit = true;
        sProcessThread.join();

        DeleteCriticalSection(&gRecordingToggleCS);
    }
}

#endif

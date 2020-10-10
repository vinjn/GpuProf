#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <shellapi.h>
#include <TlHelp32.h>
#include <evntcons.h> // must include after windows.h
#include <unordered_map>

#include "etw_prof.h"
#include "../3rdparty/PresentMon/PresentData/TraceSession.hpp"
#include "../3rdparty/PresentMon/PresentData/PresentMonTraceConsumer.hpp"
#include "../3rdparty/PresentMon/PresentData/MixedRealityTraceConsumer.hpp"
#include "../3rdparty/PresentMon/PresentMon/PresentMon.hpp"
#include <Shlwapi.h>

#pragma comment(lib, "Tdh")
#include <VersionHelpers.h>

#include "metrics_info.h"
using namespace cimg_library;
using namespace std;

#define printf(...) 

const char* blackList[] =
{
    "dwm",
    "devenv",
    "chrome",
    "explorer",
    "StartMenuExperienceHost",
    "SearchUI",
    "Code",
};

struct EtwInfo
{
    MetricsInfo metrics;
    shared_ptr<CImgDisplay> window;
    int metricId = 0;

    // Structures to track processes and statistics from recorded events.
    LateStageReprojectionData lsrData;
    std::vector<ProcessEvent> processEvents;
    std::vector<std::shared_ptr<PresentEvent>> presentEvents;
    std::vector<std::shared_ptr<LateStageReprojectionEvent>> lsrEvents;
    std::vector<uint64_t> recordingToggleHistory;
    std::vector<std::pair<uint32_t, uint64_t>> terminatedProcesses;

    bool setup()
    {
        //ElevatePrivilege(argc, argv);
        // Start the ETW trace session (including consumer and output threads).

        processEvents.reserve(128);
        presentEvents.reserve(4096);
        lsrEvents.reserve(4096);
        recordingToggleHistory.reserve(16);
        terminatedProcesses.reserve(16);

        return true;
    }

    int update();

    int draw()
    {
        // Create and display the image of the intensity profile
        CImg<unsigned char> img(window->width(), window->height(), 1, 3, 50);
        img.draw_grid(-50 * 100.0f / window->width(), -50 * 100.0f / 256, 0, 0, false, true, colors[0], 0.2f, 0xCCCCCCCC, 0xCCCCCCCC);

        metrics.draw(window, img, METRIC_FPS_0, METRIC_FPS_0 + metricId - 1, true);

        img.display(*window);

        return 0;
    }
};

static EtwInfo etwInfo;

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

    typedef BOOL(WINAPI* OpenProcessTokenProc)(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle);
    typedef BOOL(WINAPI* GetTokenInformationProc)(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass, LPVOID TokenInformation, DWORD TokenInformationLength, DWORD* ReturnLength);
    typedef BOOL(WINAPI* LookupPrivilegeValueAProc)(LPCSTR lpSystemName, LPCSTR lpName, PLUID lpLuid);
    typedef BOOL(WINAPI* AdjustTokenPrivilegesProc)(HANDLE TokenHandle, BOOL DisableAllPrivileges, PTOKEN_PRIVILEGES NewState, DWORD BufferLength, PTOKEN_PRIVILEGES PreviousState, PDWORD ReturnLength);

    struct Advapi {
        HMODULE HModule;
        OpenProcessTokenProc OpenProcessToken;
        GetTokenInformationProc GetTokenInformation;
        LookupPrivilegeValueAProc LookupPrivilegeValueA;
        AdjustTokenPrivilegesProc AdjustTokenPrivileges;

        Advapi()
            : HModule(NULL)
        {
        }

        ~Advapi()
        {
            if (HModule != NULL) {
                FreeLibrary(HModule);
            }
        }

        bool Load()
        {
            HModule = LoadLibraryA("advapi32.dll");
            if (HModule == NULL) {
                return false;
            }

            OpenProcessToken = (OpenProcessTokenProc)GetProcAddress(HModule, "OpenProcessToken");
            GetTokenInformation = (GetTokenInformationProc)GetProcAddress(HModule, "GetTokenInformation");
            LookupPrivilegeValueA = (LookupPrivilegeValueAProc)GetProcAddress(HModule, "LookupPrivilegeValueA");
            AdjustTokenPrivileges = (AdjustTokenPrivilegesProc)GetProcAddress(HModule, "AdjustTokenPrivileges");

            if (OpenProcessToken == nullptr ||
                GetTokenInformation == nullptr ||
                LookupPrivilegeValueA == nullptr ||
                AdjustTokenPrivileges == nullptr) {
                FreeLibrary(HModule);
                HModule = NULL;
                return false;
            }

            return true;
        }

        bool HasElevatedPrivilege() const
        {
            HANDLE hToken = NULL;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
                return false;
            }

            /** BEGIN WORKAROUND: struct TOKEN_ELEVATION and enum value TokenElevation
             * are not defined in the vs2003 headers, so we reproduce them here. **/
            enum { WA_TokenElevation = 20 };
            DWORD TokenIsElevated = 0;
            /** END WA **/

            DWORD dwSize = 0;
            if (!GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)WA_TokenElevation, &TokenIsElevated, sizeof(TokenIsElevated), &dwSize)) {
                TokenIsElevated = 0;
            }

            CloseHandle(hToken);

            return TokenIsElevated != 0;
        }

        bool EnableDebugPrivilege() const
        {
            HANDLE hToken = NULL;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
                return false;
            }

            TOKEN_PRIVILEGES tp = {};
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            bool enabled =
                LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &tp.Privileges[0].Luid) &&
                AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr) &&
                GetLastError() != ERROR_NOT_ALL_ASSIGNED;

            CloseHandle(hToken);

            return enabled;
        }
    };

    int RestartAsAdministrator(
        int argc,
        char** argv)
    {
        char exe_path[MAX_PATH] = {};
        GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));

        // Combine arguments into single array
        char args[1024] = {};
        for (int idx = 0, i = 1; i < argc && (size_t)idx < sizeof(args); ++i) {
            if (idx >= sizeof(args)) {
                fprintf(stderr, "internal error: command line arguments too long.\n");
                return false; // was truncated
            }

            if (argv[i][0] != '\"' && strchr(argv[i], ' ')) {
                idx += snprintf(args + idx, sizeof(args) - idx, " \"%s\"", argv[i]);
            }
            else {
                idx += snprintf(args + idx, sizeof(args) - idx, " %s", argv[i]);
            }
        }

        SHELLEXECUTEINFOA info = {};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOCLOSEPROCESS;
        info.lpVerb = "runas";
        info.lpFile = exe_path;
        info.lpParameters = args;
        info.nShow = SW_SHOW;
        if (!ShellExecuteExA(&info) || info.hProcess == NULL) {
            fprintf(stderr, "error: failed to elevate privilege ");
            int e = GetLastError();
            switch (e) {
            case ERROR_FILE_NOT_FOUND:    fprintf(stderr, "(file not found).\n"); break;
            case ERROR_PATH_NOT_FOUND:    fprintf(stderr, "(path not found).\n"); break;
            case ERROR_DLL_NOT_FOUND:     fprintf(stderr, "(dll not found).\n"); break;
            case ERROR_ACCESS_DENIED:     fprintf(stderr, "(access denied).\n"); break;
            case ERROR_CANCELLED:         fprintf(stderr, "(cancelled).\n"); break;
            case ERROR_NOT_ENOUGH_MEMORY: fprintf(stderr, "(out of memory).\n"); break;
            case ERROR_SHARING_VIOLATION: fprintf(stderr, "(sharing violation).\n"); break;
            default:                      fprintf(stderr, "(%u).\n", e); break;
            }
            return 2;
        }

        WaitForSingleObject(info.hProcess, INFINITE);

        DWORD code = 0;
        GetExitCodeProcess(info.hProcess, &code);
        int e = GetLastError();
        (void)e;
        CloseHandle(info.hProcess);

        return code;
    }
}
// Returning from this function means keep running in this process.
void ElevatePrivilege(int argc, char** argv)
{
    // Try to load advapi to check and set required privilege.
    Advapi advapi;
    if (advapi.Load() && advapi.EnableDebugPrivilege()) {
        return;
    }

    // Try to restart PresentMon with admin privileve
    exit(RestartAsAdministrator(argc, argv));
}

void StartOutputThread();
void StopOutputThread();

void Consume(TRACEHANDLE traceHandle)
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


void CheckLostReports(ULONG* eventsLost, ULONG* buffersLost)
{
    auto status = gSession.CheckLostReports(eventsLost, buffersLost);
    (void)status;
}

void DequeueAnalyzedInfo(
    std::vector<ProcessEvent>* processEvents,
    std::vector<std::shared_ptr<PresentEvent>>* presentEvents,
    std::vector<std::shared_ptr<LateStageReprojectionEvent>>* lsrEvents)
{
    gPMConsumer->DequeueProcessEvents(*processEvents);
    gPMConsumer->DequeuePresentEvents(*presentEvents);
    (lsrEvents);
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
    if (gIsRecording == record) {
        return;
    }

    uint64_t qpc = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&qpc);

    EnterCriticalSection(&gRecordingToggleCS);
    gRecordingToggleHistory.emplace_back(qpc);
    gIsRecording = record;
    LeaveCriticalSection(&gRecordingToggleCS);
}

bool CopyRecordingToggleHistory(std::vector<uint64_t>* recordingToggleHistory)
{
    EnterCriticalSection(&gRecordingToggleCS);
    recordingToggleHistory->assign(gRecordingToggleHistory.begin(), gRecordingToggleHistory.end());
    auto isRecording = gIsRecording;
    LeaveCriticalSection(&gRecordingToggleCS);

    auto recording = recordingToggleHistory->size() + (isRecording ? 1 : 0);
    return (recording & 1) == 1;
}

// Remove recording toggle events that we've processed.
void UpdateRecordingToggles(size_t nextIndex)
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

std::unordered_map<uint32_t, ProcessInfo> gProcesses;
uint32_t gTargetProcessCount = 0;

bool IsTargetProcess(uint32_t processId, std::string const& processName)
{
    return true;
#if 0
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

    return false;
#endif
}

void InitProcessInfo(ProcessInfo* processInfo, uint32_t processId, HANDLE handle, std::string const& processName)
{
    auto target = IsTargetProcess(processId, processName);

    processInfo->mHandle = handle;
    processInfo->mModuleName = processName;
    processInfo->mTargetProcess = target;

    if (target) {
        gTargetProcessCount += 1;
    }
}

extern PROCESSENTRY32 getEntryFromPID(DWORD pid);

ProcessInfo* GetProcessInfo(uint32_t processId)
{
    auto result = gProcesses.emplace(processId, ProcessInfo());
    auto processInfo = &result.first->second;
    auto newProcess = result.second;

    if (newProcess) {
        // In ETL capture, we should have gotten an NTProcessEvent for this
        // process updated via UpdateNTProcesses(), so this path should only
        // happen in realtime capture.
        HANDLE handle = NULL;
        char const* processName = "<error>";
#if 0
        char path[MAX_PATH];
        DWORD numChars = sizeof(path);
        {
            DWORD numChars = sizeof(path);
            handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
            if (QueryFullProcessImageNameA(handle, 0, path, &numChars)) {
                processName = PathFindFileNameA(path);
            }
        }
#else
        auto entry = getEntryFromPID(processId);
        processName = entry.szExeFile;
#endif

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
void CheckForTerminatedRealtimeProcesses(std::vector<std::pair<uint32_t, uint64_t>>* terminatedProcesses)
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

void HandleTerminatedProcess(uint32_t processId)
{
    auto iter = gProcesses.find(processId);
    if (iter == gProcesses.end()) {
        return; // shouldn't happen.
    }

    gProcesses.erase(iter);
}

void UpdateProcesses(std::vector<ProcessEvent> const& processEvents, std::vector<std::pair<uint32_t, uint64_t>>* terminatedProcesses)
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

void AddPresents(std::vector<std::shared_ptr<PresentEvent>> const& presentEvents, size_t* presentEventIndex,
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
            //UpdateCsv(processInfo, *chain, *presentEvent);
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

// Limit the present history stored in SwapChainData to 2 seconds.
void PruneHistory(
    std::vector<ProcessEvent> const& processEvents,
    std::vector<std::shared_ptr<PresentEvent>> const& presentEvents,
    std::vector<std::shared_ptr<LateStageReprojectionEvent>> const& lsrEvents)
{
    assert(processEvents.size() + presentEvents.size() + lsrEvents.size() > 0);

    auto latestQpc = std::max<uint64_t>(std::max<uint64_t>(
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
        if (!hitNextRecordingToggle) {
            break;
        }

        // Toggle recording.
        recordingToggleIndex += 1;
        recording = !recording;
        if (!recording) {
#if 0
            IncrementRecordingCount();
            CloseOutputCsv(nullptr);
            for (auto& pair : gProcesses) {
                CloseOutputCsv(&pair.second);
            }
#endif
        }
    }

done:

    // Limit the present history stored in SwapChainData to 2 seconds, so that
    // processes that stop presenting are removed from the console display.
    // This only applies to ConsoleOutput::Full, otherwise it's ok to just
    // leave the older presents in the history buffer since they aren't used
    // for anything.
    // TODO: check
    //if (args.mConsoleOutputType == ConsoleOutput::Full) {
    PruneHistory(*processEvents, *presentEvents, *lsrEvents);
    //}

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
        //ExitMainThread();
    }
}

const char* RuntimeToString(Runtime rt)
{
    switch (rt) {
    case Runtime::DXGI: return "DXGI";
    case Runtime::D3D9: return "D3D9";
    default: return "Other";
    }
}

const char* PresentModeToString(PresentMode mode)
{
    switch (mode) {
    case PresentMode::Hardware_Legacy_Flip: return "Hardware: Legacy Flip";
    case PresentMode::Hardware_Legacy_Copy_To_Front_Buffer: return "Hardware: Legacy Copy to front buffer";
    case PresentMode::Hardware_Independent_Flip: return "Hardware: Independent Flip";
    case PresentMode::Composed_Flip: return "Composed: Flip";
    case PresentMode::Composed_Copy_GPU_GDI: return "Composed: Copy with GPU GDI";
    case PresentMode::Composed_Copy_CPU_GDI: return "Composed: Copy with CPU GDI";
    case PresentMode::Composed_Composition_Atlas: return "Composed: Composition Atlas";
    case PresentMode::Hardware_Composed_Independent_Flip: return "Hardware Composed: Independent Flip";
    default: return "Other";
    }
}

void UpdateMetrics(uint32_t processId, ProcessInfo const& processInfo)
{
    // Don't display non-target or empty processes
    if (!processInfo.mTargetProcess ||
        processInfo.mModuleName.empty() ||
        processInfo.mSwapChain.empty()) {
        return;
    }

    auto procName = processInfo.mModuleName;
    procName = procName.substr(0, procName.length() - 4);
    for (const auto& name : blackList)
    {
        if (procName == name)
            return;
    }

    for (auto const& pair : processInfo.mSwapChain) {
        auto address = pair.first;
        auto const& chain = pair.second;

        // Only show swapchain data if there at least two presents in the
        // history.
        if (chain.mPresentHistoryCount < 2) {
            continue;
        }

        auto const& present0 = *chain.mPresentHistory[(chain.mNextPresentIndex - chain.mPresentHistoryCount) % SwapChainData::PRESENT_HISTORY_MAX_COUNT];
        auto const& presentN = *chain.mPresentHistory[(chain.mNextPresentIndex - 1) % SwapChainData::PRESENT_HISTORY_MAX_COUNT];
        auto cpuAvg = QpcDeltaToSeconds(presentN.QpcTime - present0.QpcTime) / (chain.mPresentHistoryCount - 1);

        printf("    %016llX (%s): SyncInterval=%d Flags=%d %.2lf ms/frame (%.1lf fps",
            address,
            RuntimeToString(presentN.Runtime),
            presentN.SyncInterval,
            presentN.PresentFlags,
            1000.0 * cpuAvg,
            1.0 / cpuAvg);


        etwInfo.metrics.addMetric((MetricType)(METRIC_FPS_0 + etwInfo.metricId), 1.0 / cpuAvg);
        kMetricNames[METRIC_FPS_0 + etwInfo.metricId] = procName;

        etwInfo.metricId++;
        if (etwInfo.metricId > METRIC_FPS_5) break;

        size_t displayCount = 0;
        uint64_t latencySum = 0;
        uint64_t display0ScreenTime = 0;
        PresentEvent* displayN = nullptr;

        for (uint32_t i = 0; i < chain.mPresentHistoryCount; ++i) {
            auto const& p = chain.mPresentHistory[(chain.mNextPresentIndex - chain.mPresentHistoryCount + i) % SwapChainData::PRESENT_HISTORY_MAX_COUNT];
            if (p->FinalState == PresentResult::Presented) {
                if (displayCount == 0) {
                    display0ScreenTime = p->ScreenTime;
                }
                displayN = p.get();
                latencySum += p->ScreenTime - p->QpcTime;
                displayCount += 1;
            }
        }

        if (displayCount >= 2) {
            printf(", %.1lf fps displayed", (double)(displayCount - 1) / QpcDeltaToSeconds(displayN->ScreenTime - display0ScreenTime));
        }

        if (displayCount >= 1) {
            printf(", %.2lf ms latency", 1000.0 * QpcDeltaToSeconds(latencySum) / displayCount);
        }

        printf(")");

        if (displayCount > 0) {
            printf(" %s", PresentModeToString(displayN->PresentMode));
        }

        printf("\n");
    }
}

void Output()
{
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
        etwInfo.metricId = 0; // reset id
        for (auto const& pair : gProcesses) {
            UpdateMetrics(pair.first, pair.second);
        }
        //UpdateMetrics(gProcesses, lsrData);

        if (realtimeRecording) {
            printf("** RECORDING **\n");
        }
        //CommitConsole();
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
        //CloseOutputCsv(processInfo);
    }
    gProcesses.clear();
}

void StartOutputThread()
{
    InitializeCriticalSection(&gRecordingToggleCS);

    //sProcessThread = std::thread(Output);
}

void StopOutputThread()
{
    if (sProcessThread.joinable()) {
        gQuit = true;
        sProcessThread.join();

        DeleteCriticalSection(&gRecordingToggleCS);
    }
}

extern vector<shared_ptr<CImgDisplay>> windows;

int etw_setup()
{
    etwInfo.setup();

    etwInfo.window = make_shared<CImgDisplay>(WINDOW_W, WINDOW_H, "FPS", 3);
    windows.push_back(etwInfo.window);

    auto simple = false;
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
            //fprintf(stderr,
            //    "warning: a trace session named \"%s\" is already running and it will be stopped.\n"
            //    "         Use -session_name with a different name to start a new session.\n",
            //    mSessionName);
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
    StartOutputThread();

    return 0;
}

int etw_cleanup()
{
    // Stop the trace session.
    gSession.Stop();

    // Wait for the consumer and output threads to end (which are using the
    // consumers).
    WaitForConsumerThreadToExit();
    StopOutputThread();

    // Destruct the consumers
    delete gPMConsumer;
    gPMConsumer = nullptr;

    return 0;
}


int etw_update()
{
    return etwInfo.update();
}

int etw_draw()
{
    return etwInfo.draw();
}


int EtwInfo::update()
{
    // Copy and process all the collected events, and update the various
    // tracking and statistics data structures.
    ProcessEvents(&lsrData, &processEvents, &presentEvents, &lsrEvents, &recordingToggleHistory, &terminatedProcesses);

    // Display information to console if requested.  If debug build and
    // simple console, print a heartbeat if recording.
    //
    // gIsRecording is the real timeline recording state.  Because we're
    // just reading it without correlation to gRecordingToggleHistory, we
    // don't need the critical section.
    auto realtimeRecording = gIsRecording;
    etwInfo.metricId = 0; // reset id
    for (auto const& pair : gProcesses) {
        UpdateMetrics(pair.first, pair.second);
    }
    // Update tracking information.
    CheckForTerminatedRealtimeProcesses(&terminatedProcesses);

    return 0;
}

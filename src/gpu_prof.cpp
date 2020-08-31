/*
 * MIT License
 *
 * Copyright (c) 2016 Jeremy Main (jmain.jp@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _HAS_STD_BYTE 0
#define GPU_PROF_VERSION "0.6"

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <assert.h>
#include <memory>
#include <string>

#include "nvidia_prof.h"
#include "intel_prof.h"
#include "amd_prof.h"

using namespace std;

#include "../3rdparty/CImg.h"
using namespace cimg_library;

bool isCanvasVisible = true;

#define WINDOW_W 640
#define WINDOW_H 240

vector<shared_ptr<CImgDisplay>> windows;

enum MetricType
{
    METRIC_SM_SOL,
    METRIC_MEM_SOL,
    METRIC_FB_USAGE,
    METRIC_NVENC_SOL,
    METRIC_NVDEC_SOL,
    METRIC_SM_CLK,
    METRIC_MEM_CLK,
    METRIC_PCIE_TX,
    METRIC_PCIE_RX,
    METRIC_NVLINK_TX,
    METRIC_NVLINK_RX,

    METRIC_COUNT,
};

char* kMetricNames[] =
{
    "SM",
    "MEM",
    "FB",
    "ENC",
    "DEC",
    "SM CLK",
    "MEM CLK",
    "PCIE TX",
    "PCIE RX",
    "NVLK TX",
    "NVLK RX",
};

const uint8_t colors[][3] =
{
    { 255,255,255 },
    { 195,38,114 },
    { 69,203,209},
    { 138,226,36 },
    { 174,122,169 },
    { 200,122,10 },
    { 122,200,10 },
    { 10,122,200 },
    { 122,122,122 },
    { 200,122,10 },
    { 10,122,200 },
};


#ifdef WIN32
#include <Windows.h>
#include <tlhelp32.h>

#if 0
#include "C:/Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/km/d3dkmthk.h"

DWORD WINAPI vsyncThread(LPVOID lpParam) {
    printf("+vsyncThread()\n");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    LPFND3DKMT_OPENADAPTERFROMHDC lpfnKTOpenAdapterFromHdc = (LPFND3DKMT_OPENADAPTERFROMHDC)fnBind("gdi32", "D3DKMTOpenAdapterFromHdc");
    LPFND3DKMT_WAITFORVERTICALBLANKEVENT lpfnKTWaitForVerticalBlankEvent = (LPFND3DKMT_WAITFORVERTICALBLANKEVENT)fnBind("gdi32", "D3DKMTWaitForVerticalBlankEvent");
    if (lpfnKTOpenAdapterFromHdc && lpfnKTWaitForVerticalBlankEvent) {
        D3DKMT_WAITFORVERTICALBLANKEVENT we;
        bool bBound = false;
        while (bRunningTests) {
            if (!bBound) {
                D3DKMT_OPENADAPTERFROMHDC oa;
                oa.hDc = GetDC(NULL);  // NULL = primary display monitor; NOT tested with multiple monitor setup; tested/works with hAppWnd
                bBound = (S_OK == (*lpfnKTOpenAdapterFromHdc)(&oa));
                if (bBound) {
                    we.hAdapter = oa.hAdapter;
                    we.hDevice = 0;
                    we.VidPnSourceId = oa.VidPnSourceId;
                }
            }
            if (bBound && (S_OK == (*lpfnKTWaitForVerticalBlankEvent)(&we))) {
                vsyncHaveNewTimingInfo(tick());
                vsyncSignalAllWaiters();
            }
            else {
                bBound = false;
                printf("*** vsync service in recovery mode...\n");
                Sleep(1000);
            }
        }
    }
    printf("-vsyncThread()\n");
    return 0;
}
#endif

void GoToXY(int column, int line)
{
    // Create a COORD structure and fill in its members.
    // This specifies the new position of the cursor that we will set.
    COORD coord;
    coord.X = column;
    coord.Y = line;

    // Obtain a handle to the console screen buffer.
    // (You're just using the standard console, so you can use STD_OUTPUT_HANDLE
    // in conjunction with the GetStdHandle() to retrieve the handle.)
    // Note that because it is a standard handle, we don't need to close it.
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // Finally, call the SetConsoleCursorPosition function.
    if (!SetConsoleCursorPosition(hConsole, coord))
    {
        // Uh-oh! The function call failed, so you need to handle the error.
        // You can call GetLastError() to get a more specific error code.
        // ...
    }
}

PROCESSENTRY32 getEntryFromPID(DWORD pid)
{
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (pe32.th32ProcessID == pid)
            {
                CloseHandle(hSnapshot);
                return pe32;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    return pe32;
}

#endif

struct ProcessInfo
{
    unsigned int pid;
    string exeName;
    PROCESSENTRY32 cpuStats;
    nvmlAccountingStats_t gpuStats;
};

struct GpuInfo
{
    nvmlDevice_t handle = NULL;
    nvmlPciInfo_t pciInfo;
    nvmlDriverModel_t driverModel, pendingDriverModel;
    char cDevicename[NVML_DEVICE_NAME_BUFFER_SIZE] = { '\0' };
    uint32_t numLinks;
    nvmlEnableState_t nvlinkActives[NVML_NVLINK_MAX_LINKS];
    uint32_t nvlinkMaxSpeeds[NVML_NVLINK_MAX_LINKS];
    nvmlPciInfo_t nvlinkPciInfos[NVML_NVLINK_MAX_LINKS];

    vector<ProcessInfo> processInfos;

    // Flags to denote unsupported queries
    bool bGPUUtilSupported = true;
    bool bEncoderUtilSupported = true;
    bool bDecoderUtilSupported = true;

    static const int HISTORY_COUNT = WINDOW_W / 2;
    float metrics[METRIC_COUNT][HISTORY_COUNT] = {};
    float metrics_sum[METRIC_COUNT] = {};
    float metrics_avg[METRIC_COUNT] = {};
    void addMetric(MetricType type, float value)
    {
        metrics_sum[type] -= metrics[type][0];
        metrics_sum[type] += value;
        metrics_avg[type] = metrics_sum[type] / HISTORY_COUNT;
        for (int i = 0; i < HISTORY_COUNT - 1; i++)
            metrics[type][i] = metrics[type][i + 1];
        metrics[type][HISTORY_COUNT - 1] = value;
    }
};

vector<GpuInfo> gpuInfos;

#define CHECK_NVML(nvRetValue, func) \
            if (NVML_SUCCESS != nvRetValue) \
            { \
                if (nvRetValue != NVML_ERROR_NO_PERMISSION) \
                { \
                    ShowErrorDetails(nvRetValue, #func); \
                    nvmlShutdown(); \
                    return -1; \
                } \
            }

int getUInt(nvmlDevice_t device, int fieldId, uint32_t* value)
{
    nvmlFieldValue_t fieldValue = {};
    fieldValue.fieldId = fieldId;
    nvmlReturn_t nvRetValue = NVML_ERROR_UNKNOWN;
    nvRetValue = nvmlDeviceGetFieldValues(device, 1, &fieldValue);
    *value = 0;
    CHECK_NVML(nvRetValue, nvmlDeviceGetFieldValues);
    *value = fieldValue.value.uiVal;

    return 0;
}

bool running = true;
uint32_t uiNumGPUs = 0;

int setup()
{
    char driverVersion[80];
    int cudaVersion = 0;
    auto nvRetValue = nvmlSystemGetDriverVersion(driverVersion, 80);
    nvRetValue = nvmlSystemGetCudaDriverVersion(&cudaVersion);
    printf("Driver Version: %s     CUDA Version: %d.%d\n", driverVersion, cudaVersion / 1000, cudaVersion - cudaVersion / 1000 * 1000);
    printf("------------------------------------------------------------\n");

    // Get the number of GPUs
    nvRetValue = nvmlDeviceGetCount(&uiNumGPUs);
    CHECK_NVML(nvRetValue, nvmlDeviceGetCount);

    // In the case that no GPUs were detected
    if (0 == uiNumGPUs)
    {
        printf("No NVIDIA GPUs were detected.\n");
        nvmlShutdown();
        return -1;
    }

    bool bNVLinkSupported = false;

    printf("GPU\tMODE\tNAME\n");

    gpuInfos.resize(uiNumGPUs);

    for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
    {
        auto& info = gpuInfos[iDevIDX];
        nvRetValue = nvmlDeviceGetHandleByIndex(iDevIDX, &info.handle);
        CHECK_NVML(nvRetValue, nvmlDeviceGetHandleByIndex);

        nvRetValue = nvmlDeviceSetAccountingMode(info.handle, NVML_FEATURE_ENABLED);

        printf("%d", iDevIDX);
        nvRetValue = nvmlDeviceGetPciInfo(info.handle, &info.pciInfo);
        CHECK_NVML(nvRetValue, nvmlDeviceGetPciInfo);

        // nvlink
        getUInt(info.handle, NVML_FI_DEV_NVLINK_LINK_COUNT, &info.numLinks);
        assert(info.numLinks <= NVML_NVLINK_MAX_LINKS);
        for (int j = 0; j < info.numLinks; j++)
        {
            bNVLinkSupported = true;
            nvmlDeviceGetNvLinkState(info.handle, j, &info.nvlinkActives[j]);
            nvmlDeviceGetNvLinkRemotePciInfo(info.handle, j, &info.nvlinkPciInfos[j]);
            getUInt(info.handle, NVML_FI_DEV_NVLINK_SPEED_MBPS_L0 + j, &info.nvlinkMaxSpeeds[j]);

            for (int counter = 0; counter < 2; counter++)
            {
                unsigned int reset = 1;
                nvmlNvLinkUtilizationControl_t control = { NVML_NVLINK_COUNTER_UNIT_BYTES, NVML_NVLINK_COUNTER_PKTFILTER_ALL };
                nvRetValue = nvmlDeviceSetNvLinkUtilizationControl(info.handle, j, counter, &control, reset);
                //CHECK_NVML(nvRetValue, nvmlDeviceSetNvLinkUtilizationControl);
            }
        }

        // Get driver mode, WDDM or TCC?
        nvRetValue = nvmlDeviceGetDriverModel(info.handle, &info.driverModel, &info.pendingDriverModel);
        CHECK_NVML(nvRetValue, nvmlDeviceGetDriverModel);
        static char* driverModelsString[] = { "WDDM", "TCC", "N/A" };
        printf("\t%s", driverModelsString[info.driverModel]);

        // Get the device name
        nvRetValue = nvmlDeviceGetName(info.handle, info.cDevicename, NVML_DEVICE_NAME_BUFFER_SIZE);
        CHECK_NVML(nvRetValue, nvmlDeviceGetName);
        printf("\t%s\n", info.cDevicename);

        auto window = make_shared<CImgDisplay>(WINDOW_W, WINDOW_H, info.cDevicename, 3);
        windows.push_back(window);
    }
    printf("------------------------------------------------------------\n");

    // Print out a header for the utilization output
    printf("GPU\tSM\tMEM\tFBuffer(MB)\tSM-CLK\tMEM-CLK\tPCIE-TX\tPCIE-RX");
    if (bNVLinkSupported)
        printf("\tNVLK-TX\tNVLK-RX");
    printf("\n");
    printf("#id\t%%\t%%\tUsed / All\tMHz\tMHz\tMB\tMB");
    if (bNVLinkSupported)
        printf("\tMB\tMB");
    printf("\n");

    return 0;
}

int update()
{
    nvmlReturn_t nvRetValue = NVML_ERROR_UNINITIALIZED;
// Iterate through all of the GPUs
    for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
    {
        auto& info = gpuInfos[iDevIDX];
        GoToXY(0, iDevIDX + 5 + uiNumGPUs + 2);
        // Get the GPU device handle
        nvmlDevice_t handle = info.handle;

        // NOTE: nvUtil.memory is the memory controller utilization not the frame buffer utilization
        nvmlUtilization_t nvUtilData;
        nvRetValue = nvmlDeviceGetUtilizationRates(handle, &nvUtilData);
        if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
        {
            info.bGPUUtilSupported = false;
        }
        else CHECK_NVML(nvRetValue, nvmlDeviceGetUtilizationRates);

        // Get the GPU frame buffer memory information
        nvmlMemory_t GPUmemoryInfo = {};
        nvRetValue = nvmlDeviceGetMemoryInfo(handle, &GPUmemoryInfo);
        CHECK_NVML(nvRetValue, nvmlDeviceGetMemoryInfo);

        // verify that the uint64_t to unsigned long cast will not result in lost data
        if (ULLONG_MAX < GPUmemoryInfo.total)
        {
            printf("ERROR: GPU memory size exceeds variable limit\n");
            nvmlShutdown();
            return NVML_ERROR_NOT_SUPPORTED;
        }

        // convert the frame buffer value to KBytes
        uint64_t ulFrameBufferTotalMBytes = (uint64_t)(GPUmemoryInfo.total / 1024L / 1024L);
        uint64_t ulFrameBufferUsedMBytes = (uint64_t)(ulFrameBufferTotalMBytes - (GPUmemoryInfo.free / 1024L / 1024L));

        // calculate the frame buffer memory utilization
        info.addMetric(METRIC_SM_SOL, nvUtilData.gpu);
        info.addMetric(METRIC_MEM_SOL, nvUtilData.memory);
        info.addMetric(METRIC_FB_USAGE, ulFrameBufferUsedMBytes * 100.0f / ulFrameBufferTotalMBytes);

        // Get the video encoder utilization (where supported)
        uint32_t uiVidEncoderUtil = 0u;
        uint32_t uiVideEncoderLastSample = 0u;
        nvRetValue = nvmlDeviceGetEncoderUtilization(handle, &uiVidEncoderUtil, &uiVideEncoderLastSample);
        if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
        {
            info.bEncoderUtilSupported = false;
        }
        else CHECK_NVML(nvRetValue, nvmlDeviceGetEncoderUtilization);

        // Get the video decoder utilization (where supported)
        uint32_t uiVidDecoderUtil = 0u;
        uint32_t uiVidDecoderLastSample = 0u;
        nvRetValue = nvmlDeviceGetDecoderUtilization(handle, &uiVidDecoderUtil, &uiVidDecoderLastSample);
        if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
        {
            info.bDecoderUtilSupported = false;
        }
        else CHECK_NVML(nvRetValue, nvmlDeviceGetEncoderUtilization);

        info.addMetric(METRIC_NVENC_SOL, uiVidEncoderUtil);
        info.addMetric(METRIC_NVDEC_SOL, uiVidDecoderUtil);

        // Clock
        uint32_t clocks[NVML_CLOCK_COUNT];
        for (int i = 0; i < NVML_CLOCK_COUNT; i++)
        {
            nvRetValue = nvmlDeviceGetClockInfo(handle, nvmlClockType_t(i), clocks + i);
            if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
            {

            }
            else CHECK_NVML(nvRetValue, nvmlDeviceGetClockInfo);
        }

        // pcie traffic
        uint32_t pcieUtils[NVML_PCIE_UTIL_COUNT];
        for (int i = 0; i < NVML_PCIE_UTIL_COUNT; i++)
        {
            nvRetValue = nvmlDeviceGetPcieThroughput(handle, nvmlPcieUtilCounter_t(i), pcieUtils + i);
            if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
            {

            }
            else CHECK_NVML(nvRetValue, nvmlDeviceGetPcieThroughput);
        }

        // Output the utilization results depending on which of the counters has data available
        // I have opted to display "-" to denote an unsupported value rather than simply display "0"
        // to clarify that the GPU/driver does not support the query. 
        printf("%d", iDevIDX);

        if (info.bGPUUtilSupported) printf("\t%d\t%d", nvUtilData.gpu, nvUtilData.memory);
        else printf("\t-\t-");
        printf("\t%lld / %lld", ulFrameBufferUsedMBytes, ulFrameBufferTotalMBytes);
        printf("\t%-5d\t%-6d", clocks[NVML_CLOCK_SM], clocks[NVML_CLOCK_MEM]);
        printf("\t%-6d\t%-6d", pcieUtils[NVML_PCIE_UTIL_TX_BYTES] / 1024L, pcieUtils[NVML_PCIE_UTIL_RX_BYTES] / 1024L);

#if 0
        if (bEncoderUtilSupported) printf("\t%d", uiVidEncoderUtil);
        else printf("\t-");
        if (bDecoderUtilSupported) printf("\t%d", uiVidDecoderUtil);
        else printf("\t-");
#endif
        //if (bNVLinkSupported)
        if (info.nvlinkActives[0])
        {
            //for (int j = 0; j < info.numLinks; j++)
            int j = 0;
            uint32_t counter = 0;
            uint64_t rxcounter = 0;
            uint64_t txcounter = 0;
            nvRetValue = nvmlDeviceGetNvLinkUtilizationCounter(info.handle, j, counter, &rxcounter, &txcounter);
            if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
            {
            }
            else
                CHECK_NVML(nvRetValue, nvmlDeviceGetNvLinkUtilizationCounter);
            rxcounter /= 1024L;
            txcounter /= 1024L;
            printf("\t%-5d\t%-5d", txcounter, rxcounter);
            info.addMetric(METRIC_NVLINK_TX, txcounter);
            info.addMetric(METRIC_NVLINK_RX, rxcounter);
        }
    }

    // Per-process info
    for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
    {
        auto& info = gpuInfos[iDevIDX];
        // Get the GPU device handle
        nvmlDevice_t handle = info.handle;
        nvmlEnableState_t mode;
        nvmlReturn_t ret = nvmlDeviceGetAccountingMode(handle, &mode);
        if (mode == NVML_FEATURE_DISABLED)
            continue;

        info.processInfos.clear();
        unsigned int pidCount = 0;
        ret = nvmlDeviceGetAccountingPids(handle, &pidCount, nullptr);
        if (pidCount > 0)
        {
            vector<unsigned int> pids(pidCount);
            ret = nvmlDeviceGetAccountingPids(handle, &pidCount, pids.data());
            CHECK_NVML(ret, nvmlDeviceGetAccountingPids);
            for (auto pid : pids)
            {
                nvmlAccountingStats_t gpuStats;
                ret = nvmlDeviceGetAccountingStats(handle, pid, &gpuStats);
                CHECK_NVML(ret, nvmlDeviceGetAccountingStats);
                if (gpuStats.isRunning && (gpuStats.gpuUtilization > 0 || gpuStats.memoryUtilization > 0))
                {
                    auto cpuStats = getEntryFromPID(pid);
                    auto p = ProcessInfo();
                    p.pid = pid;
                    p.exeName = cpuStats.szExeFile;
                    p.cpuStats = cpuStats;
                    p.gpuStats = gpuStats;
                    info.processInfos.emplace_back(p);
                }
            }
        }
    }

    // GUI
#ifdef WIN32_WITH_THIS
    SHORT state = GetAsyncKeyState(VK_SPACE);
    if (state & 1)
    {
        // LSB of state indicates it's a "CLICK"
        isCanvasVisible = !isCanvasVisible;
        if (isCanvasVisible) window->show();
        else window->close();
    }
#endif

    return NVML_SUCCESS;
}

void draw()
{
    int global_mouse_x = -1;
    int global_mouse_y = -1;
    for (auto& window : windows)
    {
        auto xm = window->mouse_x();
        auto ym = window->mouse_y();
        if (xm >= 0 && ym >= 0)
        {
            global_mouse_x = xm;
            global_mouse_y = ym;
        }
    }
    int idx = 0;
    int x0 = windows[0]->window_x();
    int y0 = windows[0]->window_y();
    for (auto& window : windows)
    {
        if (window->is_keyESC()) running = false;
        window->move(x0, y0 + idx * (window->window_height() + 30));
        // Define colors used to plot the profile, and a hatch to draw the vertical line
        unsigned int hatch = 0xF0F0F0F0;
        const auto& info = gpuInfos[idx];

        int plotType = 1;
        int vertexType = 1;
        float alpha = 0.5f;
        // Create and display the image of the intensity profile
        CImg<unsigned char> img(window->width(), window->height(), 1, 3, 50);
        img.draw_grid(-50 * 100.0f / window->width(), -50 * 100.0f / 256, 0, 0, false, true, colors[0], 0.2f, 0xCCCCCCCC, 0xCCCCCCCC);

        // metrics charts
        for (int k = METRIC_SM_SOL; k <= METRIC_NVDEC_SOL; k++)
        {
            CImg<float> plot(info.metrics[k], GpuInfo::HISTORY_COUNT, 1);
            img.draw_graph(plot, colors[k], alpha, plotType, vertexType, 100, 0);
        }

        // avg summary
        const int kFontHeight = 16;
        for (int k = METRIC_SM_SOL; k <= METRIC_NVDEC_SOL; k++)
        {
            img.draw_text(kFontHeight, kFontHeight * (k + 1),
                "avg %s: %.1f%%\n",
                colors[k], 0, 1, kFontHeight,
                kMetricNames[k],
                info.metrics_avg[k]);
        }

        // point tooltip
        if (global_mouse_x >= 0 && global_mouse_y >= 0)
        {
            auto value_idx = global_mouse_x / 2;
            for (int k = METRIC_SM_SOL; k <= METRIC_NVDEC_SOL; k++)
            {
                img.draw_text(window->window_width() - 100, kFontHeight * (k + 1),
                    "%s: %.1f%%\n",
                    colors[k], 0, 1, kFontHeight,
                    kMetricNames[k],
                    info.metrics[k][value_idx]);
            }
            img.draw_line(global_mouse_x, 0, global_mouse_x, window->height() - 1, colors[0], 0.5f, hatch = cimg::rol(hatch));
        }

        // per process info
        int k = 0;
        for (const auto& p : info.processInfos)
        {
            img.draw_text(140, kFontHeight * (k + 1),
                "%s (%d): %d%% | %d%% \n",
                colors[9], 0, 1, kFontHeight,
                p.exeName.c_str(), p.pid, p.gpuStats.gpuUtilization, p.gpuStats.memoryUtilization);
            k++;
        }
        img.display(*window);
        idx++;
    }
}

// Application entry point
int main(int argc, char* argv[])
{
#if 0
    intel_prof_main(0, NULL);
#endif

    printf("GpuProf %s from vinjn.com\n", GPU_PROF_VERSION);

    nvmlReturn_t nvRetValue = NVML_ERROR_UNINITIALIZED;

    // Before any of the NVML functions can be used nvmlInit() must be called
    nvRetValue = nvmlInit();

    if (NVML_SUCCESS != nvRetValue)
    {
        // Can not call the NVML specific error string handler if the initialization failed
        printf("[%s] error code :%d\n", "nvmlInit", nvRetValue);
        return -1;
    }

    if (setup() != 0)
        return -1;

    while (running)
    {
        if (update() != 0)
            return -1;

        if (isCanvasVisible)
        {
            draw();
        }
    }
    // Shutdown NVML
    nvRetValue = nvmlShutdown();
    CHECK_NVML(nvRetValue, nvmlShutdown);

    return (NVML_SUCCESS == nvRetValue) ? 0 : -1;
}


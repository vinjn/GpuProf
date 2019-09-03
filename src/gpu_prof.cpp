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

 // nvmlquery.cpp : Demonstrate how to load the NVML API and query GPU utilization metrics

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <assert.h>

#include "nvidia_prof.h"
#include "intel_prof.h"
#include "amd_prof.h"

using namespace std;

#include "../3rdparty/CImg.h"
using namespace cimg_library;

bool isCanvasVisible = true;

#define WINDOW_W 640
#define WINDOW_H 480

CImgDisplay window(WINDOW_W, WINDOW_H, "GpuProf", 3);
enum MetricType
{
    METRIC_SM_SOL,
    METRIC_MEM_SOL,
    METRIC_FB_USAGE,
    METRIC_SM_CLK,
    METRIC_MEM_CLK,
    METRIC_PCIE_TX,
    METRIC_PCIE_RX,

    METRIC_COUNT,
};

struct GpuInfo
{
    nvmlDevice_t handle = NULL;
    nvmlPciInfo_t pciInfo;
    nvmlDriverModel_t driverModel, pendingDriverModel;
    char cDevicename[NVML_DEVICE_NAME_BUFFER_SIZE] = { '\0' };
    uint32_t numLinks;
    nvmlEnableState_t nvlinkActives[NVML_NVLINK_MAX_LINKS];
    uint32_t nvlinkSpeeds[NVML_NVLINK_MAX_LINKS];
    nvmlPciInfo_t nvlinkPciInfos[NVML_NVLINK_MAX_LINKS];

    // Flags to denote unsupported queries
    bool bGPUUtilSupported = true;
    bool bEncoderUtilSupported = true;
    bool bDecoderUtilSupported = true;

    static const int HISTORY_COUNT = WINDOW_W / 2;
    float metrics[METRIC_COUNT][HISTORY_COUNT] = {};
    void addMetric(MetricType type, float value)
    {
        for (int i = 0; i < HISTORY_COUNT - 1; i++)
            metrics[type][i] = metrics[type][i + 1];
        metrics[type][HISTORY_COUNT - 1] = value;
    }
};

vector<GpuInfo> gpuInfos;

#ifdef WIN32
#include <Windows.h>
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
#endif


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

// Application entry point
int main(int argc, char* argv[])
{
    int iRetValue = -1;
    nvmlReturn_t nvRetValue = NVML_ERROR_UNINITIALIZED;

    // Before any of the NVML functions can be used nvmlInit() must be called
    nvRetValue = nvmlInit();

    if (NVML_SUCCESS != nvRetValue)
    {
        // Can not call the NVML specific error string handler if the initialization failed
        printf("[%s] error code :%d\n", "nvmlInit", nvRetValue);
        return iRetValue;
    }

    // Now that NVML has been initalized, before exiting normally or when handling 
    // an error condition, ensure nvmlShutdown() is called

    // For each of the GPUs detected by NVML, query the device name, GPU, GPU memory, video encoder and decoder utilization

    int cudaDriverVersion = 0;
    nvRetValue = nvmlSystemGetCudaDriverVersion(&cudaDriverVersion);

    // Get the number of GPUs
    uint32_t uiNumGPUs = 0;
    nvRetValue = nvmlDeviceGetCount(&uiNumGPUs);
    CHECK_NVML(nvRetValue, nvmlDeviceGetCount);

    // In the case that no GPUs were detected
    if (0 == uiNumGPUs)
    {
        printf("No NVIDIA GPUs were detected.\n");
        nvmlShutdown();
        return iRetValue;
    }

    bool bNVLinkSupported = false;

    printf("GPU\tMODE\tNAME\n");
    
    gpuInfos.resize(uiNumGPUs);
    for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
    {
        auto& info = gpuInfos[iDevIDX];
        nvRetValue = nvmlDeviceGetHandleByIndex(iDevIDX, &info.handle);
        CHECK_NVML(nvRetValue, nvmlDeviceGetHandleByIndex);
        printf("%d", iDevIDX);
        nvmlDeviceGetPciInfo(info.handle, &info.pciInfo);
        CHECK_NVML(nvRetValue, nvmlDeviceGetPciInfo);

        // nvlink
        getUInt(info.handle, NVML_FI_DEV_NVLINK_LINK_COUNT, &info.numLinks);
        assert(info.numLinks <= NVML_NVLINK_MAX_LINKS);
        for (int j = 0; j < info.numLinks; j++)
        {
            bNVLinkSupported = true;
            nvmlDeviceGetNvLinkState(info.handle, j, &info.nvlinkActives[j]);
            getUInt(info.handle, NVML_FI_DEV_NVLINK_SPEED_MBPS_L0 + j, &info.nvlinkSpeeds[j]);
            nvmlDeviceGetNvLinkRemotePciInfo(info.handle, j, &info.nvlinkPciInfos[j]);

            for (int counter = 0; counter < 2; counter++)
            {
                unsigned int reset = 1;
                nvmlNvLinkUtilizationControl_t control = { NVML_NVLINK_COUNTER_UNIT_BYTES, NVML_NVLINK_COUNTER_PKTFILTER_ALL };
                nvRetValue = nvmlDeviceSetNvLinkUtilizationControl(info.handle, j, counter, &control, reset);
                CHECK_NVML(nvRetValue, nvmlDeviceSetNvLinkUtilizationControl);
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
    }
    printf("============================================================\n");
  
    // Print out a header for the utilization output
    printf("GPU\tSM\tMEM\tFBuffer(MB)\tSM-CLK\tMEM-CLK\tPCIE-TX\tPCIE-RX");
    if (bNVLinkSupported)
        printf("\tNVL-TX\tNVL-RX");
    printf("\n");
    printf("#id\t%%\t%%\tUsed / All\tMHz\tMHz\tMB\tMB");
    if (bNVLinkSupported)
        printf("\tMB\tMB");
    printf("\n");

    bool running = true;
    while (running)
    {
        // CLI
        // Iterate through all of the GPUs
        for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
        {
            auto& info = gpuInfos[iDevIDX];
            GoToXY(0, iDevIDX + 2 + uiNumGPUs + 2);
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
                return iRetValue;
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
            printf("\t%d\t%d", clocks[NVML_CLOCK_SM], clocks[NVML_CLOCK_MEM]);
            printf("\t%-3d\t%-3d", pcieUtils[NVML_PCIE_UTIL_TX_BYTES] / 1024L, pcieUtils[NVML_PCIE_UTIL_RX_BYTES] / 1024L);

#if 0
            if (bEncoderUtilSupported) printf("\t%d", uiVidEncoderUtil);
            else printf("\t-");
            if (bDecoderUtilSupported) printf("\t%d", uiVidDecoderUtil);
            else printf("\t-");
#endif
            if (info.nvlinkActives[0])
            {

            }
        }
        // GUI
#ifdef WIN32_WITH_THIS
        SHORT state = GetAsyncKeyState(VK_SPACE);
        if (state & 1)
        {
            // LSB of state indicates it's a "CLICK"
            isCanvasVisible = !isCanvasVisible;
            if (isCanvasVisible) window.show();
            else window.close();
        }
#endif

        if (isCanvasVisible)
        {
            if (window.is_keyESC()) running = false;
              // Define colors used to plot the profile, and a hatch to draw the vertical line
            unsigned int hatch = 0xF0F0F0F0;
            const unsigned char
                red[] = { 122,0,0 },
                green[] = { 0,122,0 },
                blue[] = { 0,0,122 },
                black[] = { 0,0,0 };


            const auto& info = gpuInfos[0];
            CImg<float> img1(info.metrics[METRIC_SM_SOL], GpuInfo::HISTORY_COUNT, 1);
            CImg<float> img2(info.metrics[METRIC_MEM_SOL], GpuInfo::HISTORY_COUNT, 1);
            CImg<float> img3(info.metrics[METRIC_FB_USAGE], GpuInfo::HISTORY_COUNT, 1);
            int plotType = 1;
            int vertexType = 1;
            float alpha = 0.5f;
            // Create and display the image of the intensity profile
            CImg<unsigned char> img(window.width(), window.height(), 1, 3, 255);
            img.
                draw_grid(-50 * 100.0f / window.width(), -50 * 100.0f / 256, 0, 0, false, true, black, 0.2f, 0xCCCCCCCC, 0xCCCCCCCC).
                //draw_axes(0, window.width() - 1.0f, 255.0f, 0.0f, black).
                draw_graph(img1, red, alpha, plotType, vertexType, 100, 0).
                draw_graph(img2, green, alpha, plotType, vertexType, 100, 0).
                draw_graph(img3, blue, alpha, plotType, vertexType, 100, 0);

            auto xm = window.mouse_x();
            auto ym = window.mouse_y();
            if (xm >=0 && ym >= 0)
            {
                img.draw_text(30, 5, " SM: %.1f%%\nMEM: %.1f%%\n FB: %.1f%%", black, 0, 1, 16,
                    info.metrics[METRIC_SM_SOL][xm / 2], info.metrics[METRIC_MEM_SOL][xm / 2], info.metrics[METRIC_FB_USAGE][xm / 2]);
                img.draw_line(xm, 0, xm, window.height() - 1, black, 0.5f, hatch = cimg::rol(hatch));
            }
            img.display(window);
        }
    }
    // Shutdown NVML
    nvRetValue = nvmlShutdown();
    CHECK_NVML(nvRetValue, nvmlShutdown);

    iRetValue = (NVML_SUCCESS == nvRetValue) ? 0 : -1;
    return iRetValue;
}


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

//#include "../3rdparty/CImg.h"
//using namespace cimg_library;

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
                ShowErrorDetails(nvRetValue, #func); \
                nvmlShutdown(); \
                return -1; \
            }

int getUInt(nvmlDevice_t device, int fieldId, uint32_t* value)
{
    nvmlFieldValue_t fieldValue = {};
    fieldValue.fieldId = fieldId;
    nvmlReturn_t nvRetValue = NVML_ERROR_UNKNOWN;
    nvRetValue = nvmlDeviceGetFieldValues(device, 1, &fieldValue);
    CHECK_NVML(nvRetValue, nvmlDeviceGetFieldValues);

    *value = fieldValue.value.uiVal;    return 0;}

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

    if (NVML_SUCCESS != nvRetValue)
    {
        ShowErrorDetails(nvRetValue, "nvmlDeviceGetCount");
        nvmlShutdown();
        return iRetValue;
    }

    // In the case that no GPUs were detected
    if (0 == uiNumGPUs)
    {
        printf("No NVIDIA GPUs were detected.\n");
        nvmlShutdown();
        return iRetValue;
    }

    printf("GPU\tMODE\tNAME\n");

    struct GpuInfo
    {
        nvmlDevice_t handle = NULL;
        nvmlPciInfo_t pciInfo;
        nvmlDriverModel_t driverModel, pendingDriverModel;
        char cDevicename[NVML_DEVICE_NAME_BUFFER_SIZE] = { '\0' };
        uint32_t numLinks;
        nvmlEnableState_t nvlinkActive[NVML_NVLINK_MAX_LINKS];
        uint32_t nvlinkSpeeds[NVML_NVLINK_MAX_LINKS];
        nvmlPciInfo_t nvlinkPciInfos[NVML_NVLINK_MAX_LINKS];
    };
    vector<GpuInfo> gpuInfos(uiNumGPUs);
    for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
    {
        auto& info = gpuInfos[iDevIDX];
        nvRetValue = nvmlDeviceGetHandleByIndex(iDevIDX, &info.handle);
        CHECK_NVML(nvRetValue, nvmlDeviceGetHandleByIndex);
        printf("%d", iDevIDX);
        nvmlDeviceGetPciInfo(info.handle, &info.pciInfo);        CHECK_NVML(nvRetValue, nvmlDeviceGetPciInfo);

        getUInt(info.handle, NVML_FI_DEV_NVLINK_LINK_COUNT, &info.numLinks);
        assert(info.numLinks <= NVML_NVLINK_MAX_LINKS);
        for (int j = 0; j < info.numLinks; j++)
        {
            nvmlDeviceGetNvLinkState(info.handle, j, &info.nvlinkActive[j]);            getUInt(info.handle, NVML_FI_DEV_NVLINK_SPEED_MBPS_L0 + j, &info.nvlinkSpeeds[j]);            nvmlDeviceGetNvLinkRemotePciInfo(info.handle, j, &info.nvlinkPciInfos[j]);        }
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

    // Flags to denote unsupported queries
    bool bGPUUtilSupported = true;
    bool bEncoderUtilSupported = true;
    bool bDecoderUtilSupported = true;

    // Print out a header for the utilization output
    printf("GPU\tSM\tMEM\tFBuffer(MB)\tSM-CLK\tMEM-CLK\tPCIE-TX\tPCIE-RX\n");
    printf("#id\t%%\t%%\tUsed / All\tMHz\tMHz\tMB\tMB\n");

    bool running = true;
    while (running)
    {
        // Iterate through all of the GPUs
        for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
        {
            auto& info = gpuInfos[iDevIDX];
            GoToXY(0, iDevIDX + 2 + uiNumGPUs + 2);
            // Get the GPU device handle
            nvmlDevice_t handle = NULL;
            nvRetValue = nvmlDeviceGetHandleByIndex(iDevIDX, &handle);
            CHECK_NVML(nvRetValue, nvmlDeviceGetHandleByIndex);

            // NOTE: nvUtil.memory is the memory controller utilization not the frame buffer utilization
            nvmlUtilization_t nvUtilData;
            nvRetValue = nvmlDeviceGetUtilizationRates(handle, &nvUtilData);
            if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
            {
                bGPUUtilSupported = false;
            }
            else CHECK_NVML(nvRetValue, nvmlDeviceGetUtilizationRates);

            // Get the GPU frame buffer memory information
            nvmlMemory_t GPUmemoryInfo;
            ZeroMemory(&GPUmemoryInfo, sizeof(GPUmemoryInfo));
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
            double dMemUtilzation = (((double)ulFrameBufferUsedMBytes / (double)ulFrameBufferTotalMBytes) * 100.0);

            // Get the video encoder utilization (where supported)
            uint32_t uiVidEncoderUtil = 0u;
            uint32_t uiVideEncoderLastSample = 0u;
            nvRetValue = nvmlDeviceGetEncoderUtilization(handle, &uiVidEncoderUtil, &uiVideEncoderLastSample);
            if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
            {
                bEncoderUtilSupported = false;
            }
            else CHECK_NVML(nvRetValue, nvmlDeviceGetEncoderUtilization);

            // Get the video decoder utilization (where supported)
            uint32_t uiVidDecoderUtil = 0u;
            uint32_t uiVidDecoderLastSample = 0u;
            nvRetValue = nvmlDeviceGetDecoderUtilization(handle, &uiVidDecoderUtil, &uiVidDecoderLastSample);
            if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
            {
                bDecoderUtilSupported = false;
            }
            else CHECK_NVML(nvRetValue, nvmlDeviceGetEncoderUtilization);

            // Clock
            uint32_t clocks[NVML_CLOCK_COUNT];
            for (int i = 0; i < NVML_CLOCK_COUNT; i++)
            {
                nvRetValue = nvmlDeviceGetClockInfo(handle, nvmlClockType_t(i), clocks + i);
                CHECK_NVML(nvRetValue, nvmlDeviceGetClockInfo);
            }

            // pcie traffic
            uint32_t pcieUtils[NVML_PCIE_UTIL_COUNT];
            for (int i = 0; i < NVML_PCIE_UTIL_COUNT; i++)
            {
                nvRetValue = nvmlDeviceGetPcieThroughput(handle, nvmlPcieUtilCounter_t(i), pcieUtils + i);
                CHECK_NVML(nvRetValue, nvmlDeviceGetPcieThroughput);
            }

            // Output the utilization results depending on which of the counters has data available
            // I have opted to display "-" to denote an unsupported value rather than simply display "0"
            // to clarify that the GPU/driver does not support the query. 
            printf("%d", iDevIDX);

            if (bGPUUtilSupported) printf("\t%d\t%d", nvUtilData.gpu, nvUtilData.memory);
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
        }
    }
    // Shutdown NVML
    nvRetValue = nvmlShutdown();
    CHECK_NVML(nvRetValue, nvmlShutdown);

    iRetValue = (NVML_SUCCESS == nvRetValue) ? 0 : -1;
    return iRetValue;
}


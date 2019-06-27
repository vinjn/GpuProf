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

#include <Windows.h>
#include <stdio.h>

#include "nvidia_prof.h"
#include "intel_prof.h"
#include "amd_prof.h"

#include "../3rdparty/libui/ui.h"

// Application entry point
int main(int argc, char* argv[])
{
	int iRetValue = -1 ;
	nvmlReturn_t nvRetValue = NVML_ERROR_UNINITIALIZED ;

	// Before any of the NVML functions can be used nvmlInit() must be called
	nvRetValue = nvmlInit() ;

	if (NVML_SUCCESS != nvRetValue)
	{
		// Can not call the NVML specific error string handler if the initialization failed
		printf ("[%s] error code :%d\r\n", "nvmlInit", nvRetValue) ;
		return iRetValue ;
	}

	// Now that NVML has been initalized, before exiting normally or when handling 
	// an error condition, ensure nvmlShutdown() is called

	// For each of the GPUs detected by NVML, query the device name, GPU, GPU memory, video encoder and decoder utilization

	// Get the number of GPUs
	unsigned int uiNumGPUs = 0 ;
	nvRetValue = nvmlDeviceGetCount(&uiNumGPUs) ;

	if (NVML_SUCCESS != nvRetValue)
	{
		ShowErrorDetails(nvRetValue, "nvmlDeviceGetCount") ;
		nvmlShutdown() ;
		return iRetValue ;
	}

	// In the case that no GPUs were detected
	if (0 == uiNumGPUs)
	{
		printf("No NVIDIA GPUs were detected.\r\n") ;
		nvmlShutdown() ;
		return iRetValue ;
	}

	// Flags to denote unsupported queries
	bool bGPUUtilSupported = true ;
	bool bEncoderUtilSupported = true ;
	bool bDecoderUtilSupported = true ;

	// Print out a header for the utilization output
	printf("Device #, Name, GPU(%%), Frame Buffer(%%), Video Encode(%%), Video Decode(%%)\r\n") ;

	bool running = true;
	while (running)
	{
		// Iterate through all of the GPUs
		for (unsigned int iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
		{
			// Get the GPU device handle
			nvmlDevice_t nvGPUDeviceHandle = NULL;
			nvRetValue = nvmlDeviceGetHandleByIndex(iDevIDX, &nvGPUDeviceHandle);

			if (NVML_SUCCESS != nvRetValue)
			{
				ShowErrorDetails(nvRetValue, "nvmlDeviceGetHandleByIndex");
				nvmlShutdown();
				return iRetValue;
			}

			// Get the device name
			char cDevicename[NVML_DEVICE_NAME_BUFFER_SIZE] = { '\0' };
			nvRetValue = nvmlDeviceGetName(nvGPUDeviceHandle, cDevicename, NVML_DEVICE_NAME_BUFFER_SIZE);

			if (NVML_SUCCESS != nvRetValue)
			{
				ShowErrorDetails(nvRetValue, "nvmlDeviceGetName");
				nvmlShutdown();
				return iRetValue;
			}

			// NOTE: nvUtil.memory is the memory controller utilization not the frame buffer utilization
			nvmlUtilization_t nvUtilData;
			nvRetValue = nvmlDeviceGetUtilizationRates(nvGPUDeviceHandle, &nvUtilData);
			if (NVML_SUCCESS != nvRetValue)
			{
				// Where the GPU utilization is not supported, the query will return an "Not Supported", handle it and continue
				if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
				{
					bGPUUtilSupported = false;
				}
				else
				{
					ShowErrorDetails(nvRetValue, "nvmlDeviceGetUtilizationRates");
					nvmlShutdown();
					return iRetValue;
				}
			}

			// Get the GPU frame buffer memory information
			nvmlMemory_t GPUmemoryInfo;
			ZeroMemory(&GPUmemoryInfo, sizeof(GPUmemoryInfo));
			nvRetValue = nvmlDeviceGetMemoryInfo(nvGPUDeviceHandle, &GPUmemoryInfo);
			if (NVML_SUCCESS != nvRetValue)
			{
				ShowErrorDetails(nvRetValue, "nvmlDeviceGetMemoryInfo");
				nvmlShutdown();
				return iRetValue;
			}

			// compute the amount of frame buffer memory that has been used
			unsigned long long ullFrameBufferUsedBytes = 0L;
			ullFrameBufferUsedBytes = GPUmemoryInfo.total - GPUmemoryInfo.free;

			unsigned long long ulFrameBufferTotalKBytes = 0L;
			unsigned long long ulFrameBufferUsedKBytes = 0L;

			// verify that the unsigned long long to unsigned long cast will not result in lost data
			if (ULLONG_MAX < GPUmemoryInfo.total)
			{
				printf("ERROR: GPU memory size exceeds variable limit\r\n");
				nvmlShutdown();
				return iRetValue;
			}

			// convert the frame buffer value to KBytes
			ulFrameBufferTotalKBytes = (unsigned long long)(GPUmemoryInfo.total / 1024L);
			ulFrameBufferUsedKBytes = (unsigned long long)(ulFrameBufferTotalKBytes - (GPUmemoryInfo.free / 1024L));

			// calculate the frame buffer memory utilization
			double dMemUtilzation = (((double)ulFrameBufferUsedKBytes / (double)ulFrameBufferTotalKBytes) * 100.0);

			// Get the video encoder utilization (where supported)
			unsigned int uiVidEncoderUtil = 0u;
			unsigned int uiVideEncoderLastSample = 0u;
			nvRetValue = nvmlDeviceGetEncoderUtilization(nvGPUDeviceHandle, &uiVidEncoderUtil, &uiVideEncoderLastSample);
			if (NVML_SUCCESS != nvRetValue)
			{
				// Where the video encoder utilization is not supported, the query will return an "Not Supported", handle it and continue
				if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
				{
					bEncoderUtilSupported = false;
				}
				else
				{
					ShowErrorDetails(nvRetValue, "nvmlDeviceGetEncoderUtilization");
					nvmlShutdown();
					return iRetValue;
				}
			}

			// Get the video decoder utilization (where supported)
			unsigned int uiVidDecoderUtil = 0u;
			unsigned int uiVidDecoderLastSample = 0u;
			nvRetValue = nvmlDeviceGetDecoderUtilization(nvGPUDeviceHandle, &uiVidDecoderUtil, &uiVidDecoderLastSample);
			if (NVML_SUCCESS != nvRetValue)
			{
				// Where the video decoder utilization is not supported, the query will return an "Not Supported", handle it and continue
				if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
				{
					bDecoderUtilSupported = false;
				}
				else
				{
					ShowErrorDetails(nvRetValue, "nvmlDeviceGetDecoderUtilization");
					nvmlShutdown();
					return iRetValue;
				}
			}

			// Output the utilization results depending on which of the counters has data available
			// I have opted to display "-" to denote an unsupported value rather than simply display "0"
			// to clarify that the GPU/driver does not support the query. 
			if (!bEncoderUtilSupported || !bDecoderUtilSupported)
			{
				if (!bGPUUtilSupported)
				{
					printf("Device %d, %s, -, %.0f, -, -\r\n", iDevIDX, cDevicename, dMemUtilzation);
				}
				else
				{
					printf("Device %d, %s, %d, %.0f, -, -\r\n", iDevIDX, cDevicename, nvUtilData.gpu, dMemUtilzation);
				}
			}
			else
			{
				if (!bGPUUtilSupported)
				{
					printf("Device %d, %s, -, %.0f, %d, %d\r\n", iDevIDX, cDevicename, dMemUtilzation, uiVidEncoderUtil, uiVidDecoderUtil);
				}
				else
				{
					printf("Device %d, %s, %d, %.0f, %d, %d\r\n", iDevIDX, cDevicename, nvUtilData.gpu, dMemUtilzation, uiVidEncoderUtil, uiVidDecoderUtil);
				}
			}
		}
	}

	// Shutdown NVML
	nvRetValue = nvmlShutdown() ;
	if (NVML_SUCCESS != nvRetValue)
	{
		ShowErrorDetails(nvRetValue,  "nvmlShutdown") ;
	}

	// release the DLL handle
	return iRetValue ;

	iRetValue = (NVML_SUCCESS == nvRetValue) ? 0 : -1 ;  
	return iRetValue;
}


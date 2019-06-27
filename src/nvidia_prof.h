#pragma once


#include "../3rdparty/CUDA_SDK/nvml.h"

#pragma comment(lib, "../3rdparty/CUDA_SDK/nvml.lib")

// display information about the calling function and related error
void ShowErrorDetails(const nvmlReturn_t nvRetVal, const char* pFunctionName)
{
	auto pErrorDescription = nvmlErrorString(nvRetVal);
	printf("[%s] - %s\r\n", pFunctionName, pErrorDescription);
}

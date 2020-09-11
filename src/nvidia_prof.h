#pragma once


#include "../3rdparty/CUDA_SDK/nvml.h"
#define NOMINMAX
#include <windows.h>

// The default path to the NVML DLL
#define NVMLQUERY_DEFAULT_NVML_DLL_PATH "C:\\Program Files\\NVIDIA Corporation\\NVSMI\\NVML.DLL"

#define ENTRY(func) decltype(func)  *_##func = NULL;
#include "../3rdparty/CUDA_SDK/nvml.def"
#undef ENTRY(func)

bool LoadNVML()
{
	// Load the NVML DLL using the default NVML DLL install path
// NOTE: This DLL is included in the NVIDIA driver installation by default
	HINSTANCE hDLLhandle = NULL;
	hDLLhandle = LoadLibraryA(NVMLQUERY_DEFAULT_NVML_DLL_PATH);

	// if the DLL can not be found, exit
	if (NULL == hDLLhandle)
	{
		printf("NVML DLL is not installed or not found at the default path.\r\n");
		return false;
	}

#define ENTRY(func) _##func = (decltype(func)*)GetProcAddress(hDLLhandle, #func);
#include "../3rdparty/CUDA_SDK/nvml.def"
#undef ENTRY(func)

	return true;
}

// display information about the calling function and related error
void ShowErrorDetails(const nvmlReturn_t nvRetVal, const char* pFunctionName)
{
	auto pErrorDescription = _nvmlErrorString(nvRetVal);
	fprintf(stderr, "[%s] - %s\r\n", pFunctionName, pErrorDescription);
}

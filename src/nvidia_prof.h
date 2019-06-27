#pragma once


#include "../3rdparty/CUDA_SDK/nvml.h"

// The default path to the NVML DLL
#define NVMLQUERY_DEFAULT_NVML_DLL_PATH "C:\\Program Files\\NVIDIA Corporation\\NVSMI\\NVML.DLL"

// NVML Function pointer prototypes
typedef nvmlReturn_t(*PFNnvmlInit)(void);
typedef nvmlReturn_t(*PFNnvmlShutdown)(void);
typedef char*        (*PFNnvmlErrorString)(nvmlReturn_t result);
typedef nvmlReturn_t(*PFNnvmlDeviceGetCount)(unsigned int* deviceCount);
typedef nvmlReturn_t(*PFNnvmlDeviceGetHandleByIndex)(unsigned int index, nvmlDevice_t* device);
typedef nvmlReturn_t(*PFNnvmlDeviceGetUtilizationRates)(nvmlDevice_t device, nvmlUtilization_t* utilization);
typedef nvmlReturn_t(*PFNnvmlDeviceGetEncoderUtilization)(nvmlDevice_t device, unsigned int* utilization, unsigned int* samplingPeriodUs);
typedef nvmlReturn_t(*PFNnvmlDeviceGetDecoderUtilization)(nvmlDevice_t device, unsigned int* utilization, unsigned int* samplingPeriodUs);
typedef nvmlReturn_t(*PFNnvmlDeviceGetMemoryInfo)(nvmlDevice_t device, nvmlMemory_t* memory);
typedef nvmlReturn_t(*PFNnvmlDeviceGetName)(nvmlDevice_t device, char* name, unsigned int length);

// NVML Function pointer instances
PFNnvmlInit pfn_nvmlInit = NULL;
PFNnvmlShutdown pfn_nvmlShutdown = NULL;
PFNnvmlErrorString pfn_nvmlErrorString = NULL;
PFNnvmlDeviceGetCount pfn_nvmlDeviceGetCount = NULL;
PFNnvmlDeviceGetHandleByIndex pfn_nvmlDeviceGetHandleByIndex = NULL;
PFNnvmlDeviceGetUtilizationRates pfn_nvmlDeviceGetUtilizationRates = NULL;
PFNnvmlDeviceGetEncoderUtilization pfn_nvmlDeviceGetEncoderUtilization = NULL;
PFNnvmlDeviceGetDecoderUtilization pfn_nvmlDeviceGetDecoderUtilization = NULL;
PFNnvmlDeviceGetMemoryInfo pfn_nvmlDeviceGetMemoryInfo = NULL;
PFNnvmlDeviceGetName pfn_nvmlDeviceGetName = NULL;

// display information about the calling function and related error
void ShowErrorDetails(const nvmlReturn_t nvRetVal, const char* pFunctionName)
{
	char* pErrorDescription = NULL;
	pErrorDescription = pfn_nvmlErrorString(nvRetVal);
	printf("[%s] - %s\r\n", pFunctionName, pErrorDescription);
}

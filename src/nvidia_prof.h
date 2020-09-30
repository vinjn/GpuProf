#pragma once


#include "../3rdparty/CUDA_SDK/nvml.h"

#define ENTRY(func) extern decltype(func) *_##func;
#include "../3rdparty/CUDA_SDK/nvml.def"
#undef ENTRY(func)

bool LoadNVML();

// display information about the calling function and related error
void ShowErrorDetails(const nvmlReturn_t nvRetVal, const char* pFunctionName);
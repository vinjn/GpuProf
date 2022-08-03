/*
* Copyright 2014-2022 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <winternl.h>

#include <json/json.hpp>

#include "GpuDiagCommon.h"

namespace nv { namespace perf { namespace tool { namespace windows {

    using namespace nv::perf::tool;
    using namespace nlohmann;

    enum class WinVersion
    {
        Unrecognized,
        Win7_64bit,
        Win7_32bit,
        Win8_64bit,
        Win8_Arm_32bit,
        Win8_32bit,
        Win81_64bit,
        Win81_Arm_32bit,
        Win81_32bit,
        Win10_64bit,
        Win10_Arm_32bit,
        Win10_32bit,
        Win11_64bit,
    };

    struct State
    {
        SYSTEM_INFO sysInfo;

        OSVERSIONINFOEXW osInfo;
        bool isOsInfoValid = false;

        MEMORYSTATUSEX memoryInfo;
        bool isMemoryInfoValid = false;

        std::string cpuName = "Unknown";
    };

    inline const char* ToCString(WinVersion winVer)
    {
        switch (winVer)
        {
            case WinVersion::Win7_64bit:        return "Windows 7 (64 bit)";
            case WinVersion::Win7_32bit:        return "Windows 7 (32 bit)";
            case WinVersion::Win8_64bit:        return "Windows 8 (64 bit)";
            case WinVersion::Win8_Arm_32bit:    return "Windows 8 (Arm 32 bit)";
            case WinVersion::Win8_32bit:        return "Windows 8 (32 bit)";
            case WinVersion::Win81_64bit:       return "Windows 8.1 (64 bit)";
            case WinVersion::Win81_Arm_32bit:   return "Windows 8.1 (Arm 32 bit)";
            case WinVersion::Win81_32bit:       return "Windows 8.1 (32 bit)";
            case WinVersion::Win10_64bit:       return "Windows 10 (64 bit)";
            case WinVersion::Win10_Arm_32bit:   return "Windows 10 (Arm 32 bit)";
            case WinVersion::Win10_32bit:       return "Windows 10 (32 bit)";
            case WinVersion::Win11_64bit:       return "Windows 11 (64 bit)";
            default:                            return "Unrecognized";
        }
    }

    inline const char* GetProcessorArchitecture(const SYSTEM_INFO& sysInfo)
    {
        switch(sysInfo.wProcessorArchitecture)
        {
            case PROCESSOR_ARCHITECTURE_UNKNOWN:        return "Unknown";
            case PROCESSOR_ARCHITECTURE_INTEL:          return "Intel";
            case PROCESSOR_ARCHITECTURE_MIPS:           return "Mips";
            case PROCESSOR_ARCHITECTURE_ALPHA:          return "Alpha";
            case PROCESSOR_ARCHITECTURE_ALPHA64:        return "Alpha64";
            case PROCESSOR_ARCHITECTURE_PPC:            return "PowerPC";
            case PROCESSOR_ARCHITECTURE_SHX:            return "SHX";
            case PROCESSOR_ARCHITECTURE_ARM:            return "ARM";
            case PROCESSOR_ARCHITECTURE_IA64:           return "IA64";
            case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:  return "IA32 on WIN64";
            case PROCESSOR_ARCHITECTURE_AMD64:          return "AMD64";
            case PROCESSOR_ARCHITECTURE_MSIL:           return "MSIL";
            default:                                    return "Unrecognized";
        }
    }

    inline WinVersion GetOsVersion(const OSVERSIONINFOEXW& osInfo, const SYSTEM_INFO& sysInfo)
    {
        const uint32_t osMajorVersion = osInfo.dwMajorVersion;
        const uint32_t osMinorVersion = osInfo.dwMinorVersion;
        // http://msdn.microsoft.com/en-us/library/windows/desktop/ms724832%28v=vs.85%29.aspx
        if (osMajorVersion == 6)
        {
            switch (osMinorVersion)
            {
            case 1:
                if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                {
                    return WinVersion::Win7_64bit;
                }
                else
                {
                    return WinVersion::Win7_32bit;
                }
                break;
            case 2:
                if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                {
                    return WinVersion::Win8_64bit;
                }
                else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM)
                {
                    return WinVersion::Win8_Arm_32bit;
                }
                else
                {
                    return WinVersion::Win8_32bit;
                }
                break;
            case 3:
                if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                {
                    return WinVersion::Win81_64bit;
                }
                else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM)
                {
                    return WinVersion::Win81_Arm_32bit;
                }
                else
                {
                    return WinVersion::Win81_32bit;
                }
                break;
            default:
                break;
            }
        }
        else if (osMajorVersion == 10)
        {
            if (osMinorVersion == 0)
            {
                // https://docs.microsoft.com/en-us/answers/questions/586619/windows-11-build-ver-is-still-10022000194.html
                if (osInfo.dwBuildNumber >= 22000) // Windows 11
                {
                    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                    {
                        return WinVersion::Win11_64bit;
                    }
                    // unrecognized Windows version
                }
                else // Windows 10
                {
                    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
                    {
                        return WinVersion::Win10_64bit;
                    }
                    else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM)
                    {
                        return WinVersion::Win10_Arm_32bit;
                    }
                    else
                    {
                        return WinVersion::Win10_32bit;
                    }
                }
            }
        }
        NV_PERF_LOG_ERR(50, "Unrecognized OS version. Major = %u, Minor = %u, ProcessorArchitecture = %u\n", osMajorVersion, osMinorVersion, sysInfo.wProcessorArchitecture);
        return WinVersion::Unrecognized;
    }

    inline std::string GetOSString(const OSVERSIONINFOEXW& osInfo, const SYSTEM_INFO& sysInfo)
    {
        std::stringstream ss;
        ss << ToCString(GetOsVersion(osInfo, sysInfo));
        ss << " Build " << osInfo.dwBuildNumber;
        return ss.str();
    }

    inline bool InitializeState(State& state)
    {
        state = State();

        // OS version info
        {
            // GetVersion/GetVersionEx are deprecated starting in Windows8.1
            // VerifyVersionInfo() or IsWindows10OrGreater() doesn't work either without a properly versioned manifest file
            // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-verifyversioninfoa
            // https://stackoverflow.com/questions/32115255/c-how-to-detect-windows-10
            NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
            *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");
            if (!RtlGetVersion)
            {
                NV_PERF_LOG_ERR(10, "Unable to get RtlGetVersion's address\n");
            }
            else
            {
                state.osInfo.dwOSVersionInfoSize = sizeof(state.osInfo);
                if (!NT_SUCCESS(RtlGetVersion(&state.osInfo)))
                {
                    NV_PERF_LOG_ERR(10, "RtlGetVersion failed\n");
                }
                else
                {
                    state.isOsInfoValid = true;
                }
            }
        }

        // system info
        GetSystemInfo(&state.sysInfo);

        // memory info
        {
            state.memoryInfo.dwLength = sizeof(state.memoryInfo);
            if (!GlobalMemoryStatusEx(&state.memoryInfo))
            {
                NV_PERF_LOG_ERR(10, "GlobalMemoryStatusEx failed\n");
            }
            else
            {
                state.isMemoryInfoValid = true;
            }
        }

        // CPU name
        {
            // https://docs.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-160
            // Some processors support Extended Function CPUID information. When it's supported, function_id values from 0x80000000 might be used to return information.
            // Calling __cpuid with 0x80000000 as the function_id argument gets the number of the highest valid extended ID.
            int cpuInfo[4] = {};
            __cpuid(cpuInfo, 0x80000000);
            const int brandStrBeginPos = 0x80000002;
            const int highestValidExtendedId = cpuInfo[0];
            const int brandStrEndPos = highestValidExtendedId > 0x80000005 ? 0x80000005 : highestValidExtendedId;
            char brandStr[0x40] = {};
            size_t offset = 0;
            for (int ii = brandStrBeginPos; ii <= brandStrEndPos; ++ii)
            {
                __cpuid(cpuInfo, ii);
                memcpy(brandStr + offset, cpuInfo, sizeof(cpuInfo)); // re-interpreted as a list of chars
                offset += sizeof(cpuInfo);
            }
            state.cpuName = brandStr;
        }

        return true;
    }

    inline void AppendState(const State& state, ordered_json& node)
    {
        node["OS"] = nullptr;
        if (state.isOsInfoValid)
        {
            node["OS"] = GetOSString(state.osInfo, state.sysInfo);
        }
        node["Processor"] = state.cpuName;
        node["ProcessorArchitecture"] = GetProcessorArchitecture(state.sysInfo);
        node["NumberOfProcessors"] = state.sysInfo.dwNumberOfProcessors;
        node["PhysicalMemory"] = nullptr;
        if (state.isMemoryInfoValid)
        {
            node["PhysicalMemory"] = SizeToString(state.memoryInfo.ullTotalPhys);
        }
    }

    inline void CleanupState(State& state)
    {
        state = State();
    }

}}}} // nv::perf::tool::windows

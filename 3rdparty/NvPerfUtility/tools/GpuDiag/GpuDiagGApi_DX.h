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
#include <vector>
#include <cmath>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <shellapi.h>

#include <json/json.hpp>

#include <NvPerfInit.h>
#include <NvPerfD3D12.h>

#include "GpuDiagCommon.h"

namespace nv { namespace perf { namespace tool { namespace dx {

    using namespace nv::perf::tool;
    using namespace nlohmann;

    struct State
    {
        struct Device
        {
            size_t adapterIndex = size_t(~0);
            size_t nvpwDeviceIndex = size_t(~0);
            Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
            Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
            DXGI_ADAPTER_DESC1 adapterDesc = {};
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;
        };
        std::vector<Device> devices;
        bool isDriverLoaded = false;
    };

    inline bool IsDebugLayerEnabled(ID3D12Device* pDevice)
    {
        Microsoft::WRL::ComPtr<ID3D12DebugDevice> pDebugDevice;
        HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice));
        if (SUCCEEDED(hr))
        {
            return true;
        }
        return false;
    }

    inline NVPA_Status ProfilerSessionSupported(ID3D12CommandQueue* pCommandQueue)
    {
        NVPW_D3D12_Profiler_CalcTraceBufferSize_Params calcTraceBufferSizeParam = { NVPW_D3D12_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE };
        calcTraceBufferSizeParam.maxRangesPerPass = 1;
        calcTraceBufferSizeParam.avgRangeNameLength = 256;
        NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_CalcTraceBufferSize(&calcTraceBufferSizeParam);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D12_Profiler_CalcTraceBufferSize failed\n");
            return nvpaStatus;
        }

        NVPW_D3D12_Profiler_Queue_BeginSession_Params beginSessionParams = { NVPW_D3D12_Profiler_Queue_BeginSession_Params_STRUCT_SIZE };
        beginSessionParams.pCommandQueue = pCommandQueue;
        beginSessionParams.numTraceBuffers = 2;
        beginSessionParams.traceBufferSize = calcTraceBufferSizeParam.traceBufferSize;
        beginSessionParams.maxRangesPerPass = 1;
        beginSessionParams.maxLaunchesPerPass = 1;
        nvpaStatus = NVPW_D3D12_Profiler_Queue_BeginSession(&beginSessionParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D12_Profiler_Queue_BeginSession failed\n");
            return nvpaStatus;
        }

        NVPW_D3D12_Profiler_Queue_EndSession_Params endSessionParams = { NVPW_D3D12_Profiler_Queue_EndSession_Params_STRUCT_SIZE };
        endSessionParams.pCommandQueue = pCommandQueue;
        endSessionParams.timeout = INFINITE;
        NVPA_Status endSessionStatus = NVPW_D3D12_Profiler_Queue_EndSession(&endSessionParams);
        if (endSessionStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D12_Profiler_Queue_EndSession failed\n");
        }
        return nvpaStatus;
    }

    // if any of the DX calls fail, the function failes;
    // but succeeding in NVPW calls is not a must, as per the purpose of this program
    inline bool InitializeState(State& state)
    {
        HRESULT result = S_OK;
        Microsoft::WRL::ComPtr<IDXGIFactory4> pFactory;
        result = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
        if (result != S_OK)
        {
            NV_PERF_LOG_ERR(10, "CreateDXGIFactory2 failed!\n");
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
        for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &pAdapter); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 adapterDesc = {};
            result = pAdapter->GetDesc1(&adapterDesc);
            if (result != S_OK)
            {
                NV_PERF_LOG_ERR(50, "pAdapter->GetDesc1 failed for adapter index %u!\n", adapterIndex);
                return false;
            }

            State::Device device;
            device.adapterIndex = adapterIndex;
            device.adapterDesc = adapterDesc;
            device.pAdapter = std::move(pAdapter);
            pAdapter = nullptr;
            result = D3D12CreateDevice(device.pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device.pDevice));
            if (result != S_OK)
            {
                NV_PERF_LOG_ERR(10, "D3D12CreateDevice failed for adapter index %u!\n", adapterIndex);
                return false;
            }

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            result = device.pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&device.pCommandQueue));
            if (result != S_OK)
            {
                NV_PERF_LOG_ERR(10, "Create a direct queue failed for adapter index %u!\n", adapterIndex);
                // continue execution
            }
            state.devices.emplace_back(std::move(device));
        }

        // profiler-specific
        bool success = true;
        success = D3D12LoadDriver();
        if (!success)
        {
            NV_PERF_LOG_ERR(10, "D3D12LoadDriver failed!\n");
        }
        else
        {
            state.isDriverLoaded = true;
            for (size_t deviceIndex = 0; deviceIndex < state.devices.size(); ++deviceIndex)
            {
                State::Device& device = state.devices[deviceIndex];
                if (D3D12IsNvidiaDevice(device.pDevice.Get()))
                {
                    const size_t sliIndex = 0;
                    device.nvpwDeviceIndex = D3DGetNvperfDeviceIndex(device.pAdapter.Get(), sliIndex);
                    if (device.nvpwDeviceIndex == ~size_t(0))
                    {
                        NV_PERF_LOG_ERR(50, "D3DGetNvperfDeviceIndex failed for adapter index %u!\n", deviceIndex);
                    }
                }
            }
        }

        return true;
    }

    inline void AppendDeviceState(const State& state, size_t deviceIndex, ordered_json& node)
    {
        const State::Device& currentDevice = state.devices[deviceIndex];
        const DXGI_ADAPTER_DESC1& adapterDesc = currentDevice.adapterDesc;
        node["DXGIAdapterIndex"] = deviceIndex;
        // nlohmann/json doesn't yet support wchar, covert it to std::string
        // https://github.com/nlohmann/json/issues/2453
        node["Name"] = WStrToUtf8(std::wstring(adapterDesc.Description));
        node["VendorId"] = adapterDesc.VendorId;
        node["DeviceId"] = adapterDesc.DeviceId;
        {
            uint8_t luid[sizeof(adapterDesc.AdapterLuid)] = {};
            memcpy(luid, &adapterDesc.AdapterLuid.LowPart, sizeof(adapterDesc.AdapterLuid.LowPart));
            memcpy(luid + sizeof(adapterDesc.AdapterLuid.LowPart), &adapterDesc.AdapterLuid.HighPart, sizeof(adapterDesc.AdapterLuid.HighPart));
            node["DeviceLUID"] = IdToString<sizeof(adapterDesc.AdapterLuid)>(luid);
        }
        node["DedicatedVideoMemory"] = SizeToString(adapterDesc.DedicatedVideoMemory);
        node["DedicatedSystemMemory"] = SizeToString(adapterDesc.DedicatedSystemMemory);
        node["SharedSystemMemory"] = SizeToString(adapterDesc.SharedSystemMemory);
        node["IsDebugLayerForcedOn"] = IsDebugLayerEnabled(currentDevice.pDevice.Get());

        // displays
        HRESULT hr = S_OK;
        node["Displays"] = ordered_json::array();
        auto& displays = node["Displays"];
        for (uint32_t outputIdx = 0; ; ++outputIdx)
        {
            IDXGIOutput* pOutput = nullptr;
            hr = currentDevice.pAdapter->EnumOutputs(outputIdx, &pOutput);
            if (SUCCEEDED(hr))
            {
                DXGI_OUTPUT_DESC outputDesc;
                hr = pOutput->GetDesc(&outputDesc);
                if (!SUCCEEDED(hr))
                {
                    NV_PERF_LOG_ERR(10, "pOutput->GetDesc failed for outputIdx: %u!\n", outputIdx);
                    continue;
                }
                auto display = ordered_json();
                display["OutputIndex"] = outputIdx;
                display["Description"] = WStrToUtf8(std::wstring(outputDesc.DeviceName));
                display["Left"] = outputDesc.DesktopCoordinates.left;
                display["Top"] = outputDesc.DesktopCoordinates.top;
                display["Width"] = std::abs(outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left);
                display["Height"] = std::abs(outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top);
                display["AttachedToDesktop"] = !!outputDesc.AttachedToDesktop;
                displays.emplace_back(std::move(display));
            }
            else if (hr == DXGI_ERROR_NOT_FOUND)
            {
                break; // DFD
            }
            else
            {
                break;
            }
        }

        // NV-specific
        const bool isNvidiaDevice = (adapterDesc.VendorId == 0x10de);
        node["IsNvidiaDevice"] = isNvidiaDevice;
        node["ProfilerDeviceIndex"] = nullptr;
        node["ProfilerIsGpuSupported"]["IsSupported"] = false;
        node["ProfilerIsGpuSupported"]["GpuArchitectureSupported"] = nullptr;
        node["ProfilerIsGpuSupported"]["SliSupportLevel"] = nullptr;
        node["ProfilerIsGpuSupported"]["CmpSupportLevel"] = nullptr;
        node["ProfilerIsGpuSupported"]["Advice"] = "Unrecognized device";
        node["ProfilerIsSessionSupported"]["IsSupported"] = false;
        node["ProfilerIsSessionSupported"]["Advice"] = "Unsupported Gpu";
        [&]() {
            if (!isNvidiaDevice || currentDevice.nvpwDeviceIndex == ~0)
            {
                return false;
            }
            node["ProfilerDeviceIndex"] = currentDevice.nvpwDeviceIndex;

            NVPW_D3D12_Profiler_IsGpuSupported_Params isGpuSupportedParams = { NVPW_D3D12_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
            isGpuSupportedParams.deviceIndex = currentDevice.nvpwDeviceIndex;
            NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_IsGpuSupported(&isGpuSupportedParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(10, "NVPW_D3D12_Profiler_IsGpuSupported failed\n");
                return false;
            }
            node["ProfilerIsGpuSupported"]["GpuArchitectureSupported"] = true;
            node["ProfilerIsGpuSupported"]["SliSupportLevel"] = true;
            node["ProfilerIsGpuSupported"]["CmpSupportLevel"] = true;
            if (!isGpuSupportedParams.isSupported)
            {
                std::string unsupportedReason = "";
                if (isGpuSupportedParams.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
                {
                    node["ProfilerIsGpuSupported"]["GpuArchitectureSupported"] = false;
                    unsupportedReason += "Unsupported GPU architecture;";
                }
                if (isGpuSupportedParams.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
                {
                    node["ProfilerIsGpuSupported"]["SliSupportLevel"] = false;
                    unsupportedReason += "Devices in SLI configuration are not supported;";
                }
                if (isGpuSupportedParams.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
                {
                    node["ProfilerIsGpuSupported"]["CmpSupportLevel"] = false;
                    unsupportedReason += "Cryptomining GPUs (NVIDIA CMP) are not supported;";
                }
                node["ProfilerIsGpuSupported"]["Advice"] = unsupportedReason;
                return false;
            }
            node["ProfilerIsGpuSupported"]["IsSupported"] = true;
            node["ProfilerIsGpuSupported"]["Advice"] = "";

            // test if we can start a profiler session on this device
            nvpaStatus = ProfilerSessionSupported(currentDevice.pCommandQueue.Get());
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(10, "ProfilerSessionSupported failed\n");
                std::string unsupportedReason;
                if (nvpaStatus == NVPA_STATUS_INSUFFICIENT_PRIVILEGE)
                {
                    unsupportedReason = "Profiling permissions not enabled. Please follow these instructions: https://developer.nvidia.com/nvidia-development-tools-solutions-ERR_NVGPUCTRPERM-permission-issue-performance-counters";
                }
                else if (nvpaStatus == NVPA_STATUS_INSUFFICIENT_DRIVER_VERSION)
                {
                    unsupportedReason = "Insufficient driver version. Please install the latest NVIDIA driver from https://www.nvidia.com";
                }
                else
                {
                    unsupportedReason = "Unknown error";
                }
                node["ProfilerIsSessionSupported"]["Advice"] = unsupportedReason;
                return false;
            }
            node["ProfilerIsSessionSupported"]["IsSupported"] = true;
            node["ProfilerIsSessionSupported"]["Advice"] = "";
            return true;
        }();
    }

    inline void AppendState(const State& state, ordered_json& node)
    {
        node["ProfilerDriverLoaded"] = state.isDriverLoaded;
        auto& devices = node["Devices"];
        for (size_t deviceIndex = 0; deviceIndex < state.devices.size(); ++deviceIndex)
        {
            devices.emplace_back(ordered_json());
            auto& device = devices.back();
            AppendDeviceState(state, deviceIndex, device);
        }
    }

    inline void CleanupState(State& state)
    {
        state = State();
    }

}}}} // nv::perf::tool::dx

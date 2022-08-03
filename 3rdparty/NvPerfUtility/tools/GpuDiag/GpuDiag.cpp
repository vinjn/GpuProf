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

#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <cassert>

#include <nvperf_host_impl.h>
#include <NvPerfInit.h>
#include <nvperf_target.h>
#include "GpuDiagGApi_VK.h"
#if defined(_WIN32)
#include "GpuDiagGApi_DX.h"
#include "GpuDiagOS_Windows.h"
#elif defined(__linux__)
#include "GpuDiagOS_Linux.h"
#endif

namespace nv { namespace perf { namespace tool {

    using namespace nlohmann;

    extern const std::string HtmlTemplate;
    const char* DEFAULT_HTML_OUTPUT_PATH = "GpuDiag.html";

    struct Options
    {
        enum class Output
        {
            json,
            html
        };
        Output output = Output::json;
        std::string htmlPath = DEFAULT_HTML_OUTPUT_PATH;
    };

    struct GpuDiagState
    {
        vk::State vkState;
#if defined(_WIN32)
        dx::State dxState;
        windows::State winState;
#elif defined(__linux__)
        linux_::State linuxState;
#endif
    };

    static void PrintUsage()
    {
        printf("Usage: GpuDiag [--html [path_to_html_file]]\n");
        printf("\n");
        printf("By default it will print JSON to the console.\n");
        printf("Use \"--html path_to_html_file\" to generate a html file.\n");
        printf("The default \"path_to_html_file\" is %s in the current working directory.\n", DEFAULT_HTML_OUTPUT_PATH);
    }

    static bool ParseArguments(const int argc, const char* argv[], Options& options)
    {
        for(uint32_t argIdx = 1; argIdx < (uint32_t)argc; ++argIdx)
        {
            if (!strcmp(argv[argIdx], "-h") || !strcmp(argv[argIdx], "--help"))
            {
                PrintUsage();
                exit(0);
            }
        }

        options = Options{};
        if (argc == 1)
        {
            return true;
        }

        if (!strcmp(argv[1], "--html"))
        {
            options.output = Options::Output::html;
            if (argc >= 3)
            {
                options.htmlPath = argv[2];
            }
        }
        else
        {
            NV_PERF_LOG_ERR(10, "Unknown argument specified: %s\n", argv[1]);
            PrintUsage();
            return false;
        }

        return true;
    }

    static size_t GetDeviceCount()
    {
        NVPW_GetDeviceCount_Params getDeviceCountParams = { NVPW_GetDeviceCount_Params_STRUCT_SIZE };
        NVPA_Status nvpaStatus = NVPW_GetDeviceCount(&getDeviceCountParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "Failed NVPW_GetDeviceCount: %u\n", nvpaStatus);
            return size_t(~0);
        }
        return getDeviceCountParams.numDevices;
    }

    static void AppendGlobalState(const GpuDiagState& state, ordered_json& node)
    {
        {
            node["GraphicsDriverVersion"] = nullptr;
            std::string driverVersion;
            bool success = vk::GetDriverVersion(state.vkState, driverVersion);
            if (success)
            {
                node["GraphicsDriverVersion"] = driverVersion;
            }
        }

        // gpus
        node["GPUs"] = ordered_json::array();
        const size_t numDevices = GetDeviceCount();
        if (numDevices == size_t(~0))
        {
            NV_PERF_LOG_ERR(50, "Failed GetDeviceCount\n");
        }
        else 
        {
            auto& gpusNode = node["GPUs"];
            for (size_t nvpwDeviceIndex = 0; nvpwDeviceIndex < numDevices; ++nvpwDeviceIndex)
            {
                gpusNode.emplace_back(ordered_json());
                auto& gpu = gpusNode.back();

                gpu["ProfilerDeviceIndex"] = nvpwDeviceIndex;
                const DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(nvpwDeviceIndex);
                gpu["DeviceName"] = deviceIdentifiers.pDeviceName;
                gpu["ChipName"] = deviceIdentifiers.pChipName;

                // use VK as a cross-platform way for querying vRamSize/ClockStatus etc
                // first, map nvpw device index to vk device index
                size_t vkDeviceIndex = 0;
                for (; vkDeviceIndex < state.vkState.devices.size(); ++vkDeviceIndex)
                {
                    if (state.vkState.devices[vkDeviceIndex].nvpwDeviceIndex == nvpwDeviceIndex)
                    {
                        break;
                    }
                }

                gpu["VideoMemorySize"] = nullptr;
                gpu["ClockStatus"] = nullptr;
                if (vkDeviceIndex == state.vkState.devices.size())
                {
                    NV_PERF_LOG_ERR(10, "Unable to find vkDeviceIndex for nvpwDeviceIndex: %u\n", nvpwDeviceIndex);
                    continue;
                }
                const VkPhysicalDevice physicalDevice = state.vkState.devices[vkDeviceIndex].physical;
                gpu["VideoMemorySize"] = vk::SizeToString(vk::GetVRamSize(physicalDevice));
                gpu["ClockStatus"] = nv::perf::ToCString(GetDeviceClockState(nvpwDeviceIndex));
            }
        }
    }

    bool InitializeState(GpuDiagState& globalState)
    {
        bool nvperfStatus = InitializeNvPerf();
        if (!nvperfStatus)
        {
            NV_PERF_LOG_ERR(10, "InitializeNvPerf failed!\n");
            return false;
        }
        if (!vk::InitializeState(globalState.vkState))
        {
            NV_PERF_LOG_ERR(10, "vk::InitializeState failed!\n");
            return false;
        }
#if defined(_WIN32)
        if (!dx::InitializeState(globalState.dxState))
        {
            NV_PERF_LOG_ERR(10, "dx::InitializeState failed!\n");
            return false;
        }
        if (!windows::InitializeState(globalState.winState))
        {
            NV_PERF_LOG_ERR(10, "windows::InitializeState failed!\n");
            return false;
        }
#elif defined(__linux__)
        if (!linux_::InitializeState(globalState.linuxState))
        {
            NV_PERF_LOG_ERR(10, "linux_::InitializeState failed!\n");
            return false;
        }
#endif
        return true;
    }

    void AppendState(const GpuDiagState& globalState, ordered_json& root)
    {
#if defined(_WIN32)
        windows::AppendState(globalState.winState, root["Windows"]);
#elif defined(__linux__)
        linux_::AppendState(globalState.linuxState, root["Linux"]);
#endif
        AppendGlobalState(globalState, root["Global"]);
        vk::AppendState(globalState.vkState, root["Vulkan"]);
#if defined(_WIN32)
        dx::AppendState(globalState.dxState, root["D3D"]);
#endif
    }

    void CleanupState(GpuDiagState& globalState)
    {
        vk::CleanupState(globalState.vkState);
#if defined(_WIN32)
        dx::CleanupState(globalState.dxState);
        windows::CleanupState(globalState.winState);
#elif defined(__linux__)
        linux_::CleanupState(globalState.linuxState);
#endif
    }

    bool Output(const Options& options, const ordered_json& root)
    {
        const int indent = 4;
        const std::string jsonStr = root.dump(indent);
        if (options.output == Options::Output::json)
        {
            std::cout << jsonStr << std::endl;
        }
        else if (options.output == Options::Output::html)
        {
            std::ofstream html(options.htmlPath);
            NV_PERF_LOG_INF(10, "Writing a html report to %s\n", options.htmlPath.c_str());
            if (!html.is_open())
            {
                NV_PERF_LOG_ERR(10, "Failed to open file: %s\n", options.htmlPath.c_str());
                return false;
            }

            const char* pJsonReplacementMarker = "/***JSON_DATA_HERE***/";
            const size_t insertPoint = HtmlTemplate.find(pJsonReplacementMarker);
            if (insertPoint == std::string::npos)
            {
                NV_PERF_LOG_ERR(10, "Invalid HTML template!\n");
                assert(!"Invalid HTML template!");
                return false;
            }
            html << HtmlTemplate.substr(0, insertPoint);
            html << jsonStr;
            html << HtmlTemplate.substr(insertPoint + strlen(pJsonReplacementMarker));
        }
        return true;
    }

}}} // nv::perf::tool

int main(const int argc, const char* argv[])
{
    using namespace nv::perf;
    using namespace nv::perf::tool;

    Options options;
    if (!ParseArguments(argc, argv, options))
    {
        NV_PERF_LOG_ERR(10, "Failed ParseArguments\n");
        return -1;
    }

    nlohmann::ordered_json root;
    {
        GpuDiagState globalState;
        if (!InitializeState(globalState))
        {
            NV_PERF_LOG_ERR(10, "Failed InitializeState\n");
            return -1;
        }
        AppendState(globalState, root);
        CleanupState(globalState);
    }

    if (!Output(options, root))
    {
        NV_PERF_LOG_ERR(10, "Failed Output\n");
        return -1;
    }

    return 0;
}

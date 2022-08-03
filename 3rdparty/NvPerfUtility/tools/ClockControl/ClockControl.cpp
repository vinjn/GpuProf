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

#include <vulkan/vulkan.h>

#include <nvperf_host_impl.h>
#include <NvPerfInit.h>
#include <NvPerfVulkan.h>
#include <nvperf_target.h>

using namespace nv::perf;

enum class Command
{
    Invalid,
    Status,
    Lock,
    Unlock
};

struct ClockControlState
{
    Command command;
    uint32_t deviceIdx;
    uint32_t numDevices;

    // Vulkan State
    VkInstance instance;
    std::vector<VkPhysicalDevice> physicalDevices;
    std::vector<VkDevice> logicalDevices;
};

bool Initialize(ClockControlState& clockControlState)
{
    bool nvperfStatus = InitializeNvPerf();
    if (!nvperfStatus)
    {
        return false;
    }

    // *LoadDriver must be called before using the NVPW device enumeration API.  Any GAPI will do.
    //   We choose vulkan here because it is cross-platform.
    VkApplicationInfo applicationInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "ClockControl";
    applicationInfo.applicationVersion = 1;
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> instanceExtensionNames;
    if(!VulkanAppendInstanceRequiredExtensions(instanceExtensionNames, applicationInfo.apiVersion))
    {
        NV_PERF_LOG_ERR(10, "nv::perf::VulkanAppendInstanceRequiredExtensions failed!\n");
        return false;
    }

    VkInstanceCreateInfo instanceCreateInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensionNames.data();
    instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensionNames.size();

    VkResult vulkanStatus = vkCreateInstance(&instanceCreateInfo, nullptr, &clockControlState.instance);
    if (vulkanStatus != VK_SUCCESS)
    {
        NV_PERF_LOG_ERR(10, "vkCreateInstance failed!\n");
        return false;
    }

    nvperfStatus = VulkanLoadDriver(clockControlState.instance);
    if (!nvperfStatus)
    {
        return false;
    }

    vulkanStatus = vkEnumeratePhysicalDevices(clockControlState.instance, &clockControlState.numDevices, nullptr);
    if (vulkanStatus != VK_SUCCESS)
    {
        NV_PERF_LOG_ERR(10, "Using vkEnumeratePhysicalDevices to retrieve numDevices failed!\n");
        return false;
    }

    clockControlState.physicalDevices.resize(clockControlState.numDevices);
    clockControlState.logicalDevices.resize(clockControlState.numDevices);

    vulkanStatus = vkEnumeratePhysicalDevices(clockControlState.instance, &clockControlState.numDevices, clockControlState.physicalDevices.data());
    if (vulkanStatus != VK_SUCCESS)
    {
        NV_PERF_LOG_ERR(10, "Using vkEnumeratePhysicalDevices to retrieve VkPhysicalDevices failed!\n");
        return false;
    }

    VkDeviceCreateInfo deviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    for (uint32_t deviceIdx = 0; deviceIdx < clockControlState.numDevices; ++deviceIdx)
    {
        vulkanStatus = vkCreateDevice(clockControlState.physicalDevices[deviceIdx], &deviceCreateInfo, nullptr, &clockControlState.logicalDevices[deviceIdx]);
        if (vulkanStatus != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "vkCreateDevice failed for device index %u!\n", deviceIdx);
            return false;
        }
    }

    return true;
}

void Cleanup(ClockControlState& clockControlState)
{
    for (uint32_t deviceIdx = 0; deviceIdx < clockControlState.numDevices; ++deviceIdx)
    {
        vkDestroyDevice(clockControlState.logicalDevices[deviceIdx], nullptr);
    }

    vkDestroyInstance(clockControlState.instance, nullptr);
}

bool DoStatus(ClockControlState& clockControlState)
{
    for (uint32_t deviceIdx = 0; deviceIdx < clockControlState.numDevices; ++deviceIdx)
    {
        if (clockControlState.deviceIdx != (uint32_t)-1 && clockControlState.deviceIdx != deviceIdx)
        {
            continue;
        }

        bool isNvidiaDevice = VulkanIsNvidiaDevice(clockControlState.physicalDevices[deviceIdx]);
        if (!isNvidiaDevice)
        {
            const std::string deviceName = VulkanGetDeviceName(clockControlState.physicalDevices[deviceIdx]);
            printf("[%u] %s - Not a Nvidia device!\n", deviceIdx, deviceName.c_str());
        }
        else
        {
            DeviceIdentifiers deviceIdentifiers = VulkanGetDeviceIdentifiers(
                clockControlState.instance,
                clockControlState.physicalDevices[deviceIdx],
                clockControlState.logicalDevices[deviceIdx]);

            NVPW_Device_ClockStatus clockStatus = VulkanGetDeviceClockState(
                clockControlState.instance,
                clockControlState.physicalDevices[deviceIdx],
                clockControlState.logicalDevices[deviceIdx]);

            printf("[%u] %-17s - %s\n", deviceIdx, deviceIdentifiers.pDeviceName, ToCString(clockStatus));
        }
    }

    return true;
}

bool DoLockUnlock(ClockControlState& clockControlState)
{
    NVPW_Device_ClockSetting clockSetting = NVPW_DEVICE_CLOCK_SETTING_INVALID;
    std::string clockSettingStr = "invalid";
    if (clockControlState.command == Command::Lock)
    {
        clockSetting = NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP;
        clockSettingStr = "Locked to rated TDP";
    }
    else if (clockControlState.command == Command::Unlock)
    {
        clockSetting = NVPW_DEVICE_CLOCK_SETTING_DEFAULT;
        clockSettingStr = "Unlocked";
    }
    else
    {
        NV_PERF_LOG_ERR(10, "Invalid command while trying to lock/unlock clock!\n");
        return false;
    }

    for (uint32_t deviceIdx = 0; deviceIdx < clockControlState.numDevices; ++deviceIdx)
    {
        if (clockControlState.deviceIdx != (uint32_t)-1 && clockControlState.deviceIdx != deviceIdx)
        {
            continue;
        }

        bool isNvidiaDevice = VulkanIsNvidiaDevice(clockControlState.physicalDevices[deviceIdx]);
        if (!isNvidiaDevice)
        {
            const std::string deviceName = VulkanGetDeviceName(clockControlState.physicalDevices[deviceIdx]);
            printf("[%u] %s - Not an NVIDIA device!\n", deviceIdx, deviceName.c_str());
        }
        else
        {
            DeviceIdentifiers deviceIdentifiers = VulkanGetDeviceIdentifiers(
                clockControlState.instance,
                clockControlState.physicalDevices[deviceIdx],
                clockControlState.logicalDevices[deviceIdx]);


            bool success = VulkanSetDeviceClockState(
                clockControlState.instance,
                clockControlState.physicalDevices[deviceIdx],
                clockControlState.logicalDevices[deviceIdx],
                clockSetting);

            if (success)
            {
                printf("[%u] %-17s - %s\n", deviceIdx, deviceIdentifiers.pDeviceName, clockSettingStr.c_str());
            }
            else
            {
                printf("%s\n", "VulkanSetDeviceClockState failed");
            }
        }
    }
    return true;
}

void PrintUsage()
{
    printf("Usage: ClockControl <command> [deviceIdx]\n");
    printf("\n");
    printf("Allowed values for <command>:\n");
    printf("  status        - display current clock setting per requested device\n");
    printf("  lock          - lock the clock per requested device\n");
    printf("  unlock        - unlock the clock per requested device\n");
    printf("\n");
    printf("Allowed values for [Options]:\n");
    printf(" deviceIdx      - device index to set/get, default set/get all\n");
    printf("\n");
}

bool ParseArguments(const int argc, const char* argv[], ClockControlState& clockControlState)
{
    clockControlState.command = Command::Invalid;
    clockControlState.deviceIdx = (uint32_t)-1; // set all devices

    if (argc < 2)
    {
        NV_PERF_LOG_ERR(10, "Missing <command> selection!\n");
        PrintUsage();
        return false;
    }

    for(uint32_t argidx = 1; argidx < (uint32_t)argc; ++argidx)
    {
        if (!strcmp(argv[argidx], "-h") || !strcmp(argv[argidx], "--help"))
        {
            PrintUsage();
            exit(0);
        }
    }

    if (!strcmp(argv[1], "status"))
    {
        clockControlState.command = Command::Status;
    }
    else if (!strcmp(argv[1], "lock"))
    {
        clockControlState.command = Command::Lock;
    }
    else if (!strcmp(argv[1], "unlock"))
    {
        clockControlState.command = Command::Unlock;
    }
    else
    {
        NV_PERF_LOG_ERR(10, "Invalid command selected.\n");
        PrintUsage();
        return false;
    }

    if (argc < 3)
    {
        // no device index set == set all
        return true;
    }

    clockControlState.deviceIdx = (uint32_t)atoi(argv[2]);

    return true;
}

int main(const int argc, const char* argv[])
{
    ClockControlState clockControlState;
    if (!ParseArguments(argc, argv, clockControlState))
    {
        return 0;
    }

    if (!Initialize(clockControlState))
    {
        return 0;
    }

    switch (clockControlState.command)
    {
        case Command::Status:
            DoStatus(clockControlState);
            break;
        case Command::Lock:
        case Command::Unlock:
            DoLockUnlock(clockControlState);
            break;
        default:
            NV_PERF_LOG_ERR(10, "Invalid command set!\n");
            PrintUsage();
            return 1;
    }

    Cleanup(clockControlState);

    return 0;
}

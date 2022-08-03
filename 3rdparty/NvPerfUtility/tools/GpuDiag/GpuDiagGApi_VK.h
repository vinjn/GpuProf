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

#include <vulkan/vulkan.h>
#include <json/json.hpp>

#include <NvPerfInit.h>
#include <NvPerfVulkan.h>

#include "GpuDiagCommon.h"

#define NV_DRIVER_VERSION_MAJOR(vkDriverVersion) ((uint32_t)(vkDriverVersion) >> 22)
#define NV_DRIVER_VERSION_MINOR(vkDriverVersion) (((uint32_t)(vkDriverVersion) >> 14) & 0xFF)
#define NV_DRIVER_VERSION_PATCH(vkDriverVersion) ((uint32_t)(vkDriverVersion) & 0x3fff)

namespace nv { namespace perf { namespace tool { namespace vk {

    using namespace nv::perf::tool;
    using namespace nlohmann;

    struct State
    {
        struct Device
        {
            size_t vkDeviceIndex      = size_t(~0);
            size_t nvpwDeviceIndex    = size_t(~0);
            VkPhysicalDevice physical = nullptr;
            VkDevice logical          = nullptr;
            VkQueue queue             = nullptr;
        };
        VkInstance instance           = nullptr;
        std::vector<Device> devices;
        bool isDriverLoaded           = false;
    };

    inline const char* ToCString(VkPhysicalDeviceType deviceType)
    {
        switch (deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                return "Other";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return "Integrated Gpu";
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return "Discrete Gpu";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                return "Virtual Gpu";
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                return "Cpu";
            default:
                return "Unknown";
        }
    }

    inline std::string GetApiVersion(const VkPhysicalDeviceProperties& properties)
    {
        std::stringstream ss;
        ss << VK_VERSION_MAJOR(properties.apiVersion) << "." << VK_VERSION_MINOR(properties.apiVersion) << "." << VK_VERSION_PATCH(properties.apiVersion);
        return ss.str();
    }

    // only works for NV device
    inline std::string GetDriverVersion(const VkPhysicalDeviceProperties& properties)
    {
        std::stringstream ss;
        ss << NV_DRIVER_VERSION_MAJOR(properties.driverVersion) << "." << NV_DRIVER_VERSION_MINOR(properties.driverVersion) << "." << NV_DRIVER_VERSION_PATCH(properties.driverVersion);
        return ss.str();
    }

    // only works for NV device
    inline std::string GetDriverVersion(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        return GetDriverVersion(properties);
    }

    inline bool GetDriverVersion(const State& state, std::string& driverVersion)
    {
        for (const auto& device : state.devices)
        {
            if (VulkanIsNvidiaDevice(device.physical))
            {
                driverVersion = GetDriverVersion(device.physical);
                return true;
            }
        }
        return false;
    }

    inline uint64_t GetVRamSize(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        for (size_t memoryHeapIndex = 0; memoryHeapIndex < memoryProperties.memoryHeapCount; ++memoryHeapIndex)
        {
            if (memoryProperties.memoryHeaps[memoryHeapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                const uint64_t vramSize = memoryProperties.memoryHeaps[memoryHeapIndex].size;
                return vramSize;
            }
        }
        return 0;
    }

    inline VkPhysicalDeviceIDProperties GetDeviceIdProperties(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceIDProperties deviceIdProperties;
        deviceIdProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        deviceIdProperties.pNext = nullptr;

        VkPhysicalDeviceProperties2 deviceProperties;
        deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext = &deviceIdProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
        return deviceIdProperties;
    }

    inline std::string GetDeviceUUID(const VkPhysicalDeviceIDProperties& deviceIdProperties)
    {
        const std::string deviceUUID = IdToString<VK_UUID_SIZE>(deviceIdProperties.deviceUUID);
        return deviceUUID;
    }

    inline std::string GetDeviceLUID(const VkPhysicalDeviceIDProperties& deviceIdProperties)
    {
        if (!deviceIdProperties.deviceLUIDValid)
        {
            return "Unknown";
        }
        const std::string deviceLUID = IdToString<VK_LUID_SIZE>(deviceIdProperties.deviceLUID);
        return deviceLUID;
    }

    inline std::string GetDriverUUID(const VkPhysicalDeviceIDProperties& deviceIdProperties)
    {
        const std::string driverUUID = IdToString<VK_UUID_SIZE>(deviceIdProperties.driverUUID);
        return driverUUID;
    }

    inline bool GetDeviceNodeMask(const VkPhysicalDeviceIDProperties& deviceIdProperties, uint32_t nodeMask)
    {
        if (!deviceIdProperties.deviceLUIDValid)
        {
            return false;
        }
        nodeMask = deviceIdProperties.deviceNodeMask;
        return true;
    }

    inline bool GetAvailableInstanceLayerProperties(std::vector<VkLayerProperties>& properties)
    {
        uint32_t propertyCount;
        VkResult vulkanStatus = vkEnumerateInstanceLayerProperties(&propertyCount, nullptr);
        if (vulkanStatus != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(50, "vkEnumerateInstanceLayerProperties failed to retrieve the number of properties!\n");
            return false;
        }

        properties.resize(propertyCount);
        vulkanStatus = vkEnumerateInstanceLayerProperties(&propertyCount, properties.data());
        if (vulkanStatus != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(50, "vkEnumerateInstanceLayerProperties failed to retrieve properties!\n");
            return false;
        }
        return true;
    }

    inline uint32_t GetGraphicsOrComputeQueueFamilyIndex(VkPhysicalDevice physicalDevice)
    {
        uint32_t queueFamilyPropertyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
        for (uint32_t familyIndex = 0; familyIndex < queueFamilyProperties.size(); familyIndex++)
        {
            const VkQueueFlags queueFlags = queueFamilyProperties[familyIndex].queueFlags;
            if ((queueFlags & VK_QUEUE_GRAPHICS_BIT) || (queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                return familyIndex;
            }
        }
        NV_PERF_LOG_ERR(50, "Failed to find a supported queue family!\n");
        return (uint32_t)~0;
    }

    inline bool GetRequiredDeviceExtensionSupportStatus(VkInstance instance, VkPhysicalDevice physicalDevice, std::map<const char*, bool>& requiredExtensionSupportStatus)
    {
        std::vector<const char*> requiredDeviceExtensionNames;
        bool success = VulkanAppendDeviceRequiredExtensions(instance, physicalDevice, (void*)vkGetInstanceProcAddr, requiredDeviceExtensionNames);
        if (!success)
        {
            NV_PERF_LOG_ERR(50, "VulkanAppendDeviceRequiredExtensions failed!\n");
            return false;
        }

        uint32_t extCount = 0;
        VkResult vulkanStatus = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        if (vulkanStatus != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(50, "vkEnumerateDeviceExtensionProperties failed!\n");
            return false;
        }

        std::vector<VkExtensionProperties> supportedExtensions(extCount);
        vulkanStatus = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, supportedExtensions.data());
        if (vulkanStatus != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(50, "vkEnumerateDeviceExtensionProperties failed!\n");
            return false;
        }

        for (const auto& required : requiredDeviceExtensionNames)
        {
            bool supported = false;
            for (const auto& ext : supportedExtensions)
            {
                if (!strcmp(required, ext.extensionName))
                {
                    supported = true;
                    break;
                }
            }
            requiredExtensionSupportStatus[required] = supported;
        }
        return true;
    }

    inline bool GetRequiredInstanceExtensionSupportStatus(uint32_t apiVersion, std::map<const char*, bool>& requiredExtensionSupportStatus)
    {
        std::vector<const char*> requiredInstanceExtensionNames;
        bool success = VulkanAppendInstanceRequiredExtensions(requiredInstanceExtensionNames, apiVersion);
        if (!success)
        {
            NV_PERF_LOG_ERR(50, "VulkanAppendInstanceRequiredExtensions failed!\n");
            return false;
        }

        uint32_t propertyCount = 0;
        // set `pLayerName` to nullptr to enumerate extensions from Vulkan implementation components, including
        // loader, implicit layers and ICSs
        VkResult vulkanStatus = vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr);
        if (vulkanStatus != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Using vkEnumerateInstanceExtensionProperties to retrieve propertyCount failed!\n");
            return false;
        }

        std::vector<VkExtensionProperties> supportedExtensions(propertyCount);
        vulkanStatus = vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, supportedExtensions.data());
        if (vulkanStatus != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Using vkEnumerateInstanceExtensionProperties to retrieve properties failed!\n");
            return false;
        }

        for (const auto& required : requiredInstanceExtensionNames)
        {
            bool supported = false;
            for (const auto& ext : supportedExtensions)
            {
                if (!strcmp(required, ext.extensionName))
                {
                    supported = true;
                    break;
                }
            }
            requiredExtensionSupportStatus[required] = supported;
        }
        return true;
    }

    inline NVPA_Status ProfilerSessionSupported(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkQueue queue)
    {
        NVPW_VK_Profiler_CalcTraceBufferSize_Params calcTraceBufferSizeParam = { NVPW_VK_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE };
        calcTraceBufferSizeParam.maxRangesPerPass = 1;
        calcTraceBufferSizeParam.avgRangeNameLength = 256;
        NVPA_Status nvpaStatus = NVPW_VK_Profiler_CalcTraceBufferSize(&calcTraceBufferSizeParam);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_CalcTraceBufferSize failed on %s\n", VulkanGetDeviceName(physicalDevice).c_str());
            return nvpaStatus;
        }

        NVPW_VK_Profiler_Queue_BeginSession_Params beginSessionParams = { NVPW_VK_Profiler_Queue_BeginSession_Params_STRUCT_SIZE };
        beginSessionParams.instance = instance;
        beginSessionParams.physicalDevice = physicalDevice;
        beginSessionParams.device = logicalDevice;
        beginSessionParams.queue = queue;
        beginSessionParams.pfnGetInstanceProcAddr = (void*)vkGetInstanceProcAddr;
        beginSessionParams.pfnGetDeviceProcAddr = (void*)vkGetDeviceProcAddr;
        beginSessionParams.numTraceBuffers = 2;
        beginSessionParams.traceBufferSize = calcTraceBufferSizeParam.traceBufferSize;
        beginSessionParams.maxRangesPerPass = 1;
        beginSessionParams.maxLaunchesPerPass = 1;
        nvpaStatus = NVPW_VK_Profiler_Queue_BeginSession(&beginSessionParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_Queue_BeginSession failed on %s\n", VulkanGetDeviceName(physicalDevice).c_str());
            return nvpaStatus;
        }

        NVPW_VK_Profiler_Queue_EndSession_Params endSessionParams = { NVPW_VK_Profiler_Queue_EndSession_Params_STRUCT_SIZE };
        endSessionParams.queue = queue;
        endSessionParams.timeout = 0xFFFFFFFF;
        NVPA_Status endSessionStatus = NVPW_VK_Profiler_Queue_EndSession(&endSessionParams);
        if (endSessionStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_Queue_EndSession failed on %s\n", VulkanGetDeviceName(physicalDevice).c_str());
        }
        return nvpaStatus;
    }

    // if any of the VK calls fail, the function failes;
    // but succeeding in NVPW calls is not a must, as per the purpose of this program
    inline bool InitializeState(State& state)
    {
        VkResult vulkanStatus = VK_SUCCESS;
        // instance
        {
            VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
            applicationInfo.pApplicationName = "GpuDiag";
            applicationInfo.applicationVersion = 1;
            applicationInfo.apiVersion = VulkanGetInstanceApiVersion();
            VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
            instanceCreateInfo.pApplicationInfo = &applicationInfo;

            vulkanStatus = vkCreateInstance(&instanceCreateInfo, nullptr, &state.instance);
            if (vulkanStatus != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(10, "vkCreateInstance failed!\n");
                return false;
            }
        }

        // physical devices
        {
            uint32_t numPhysicalDevices = 0;
            vulkanStatus = vkEnumeratePhysicalDevices(state.instance, &numPhysicalDevices, nullptr);
            if (vulkanStatus != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(10, "Using vkEnumeratePhysicalDevices to retrieve numDevices failed!\n");
                return false;
            }

            state.devices.resize(numPhysicalDevices);
            std::vector<VkPhysicalDevice> physicalDevices(numPhysicalDevices);
            vulkanStatus = vkEnumeratePhysicalDevices(state.instance, &numPhysicalDevices, physicalDevices.data());
            if (vulkanStatus != VK_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "Using vkEnumeratePhysicalDevices to retrieve VkPhysicalDevices failed!\n");
                return false;
            }
            for (uint32_t deviceIndex = 0; deviceIndex < numPhysicalDevices; ++deviceIndex)
            {
                State::Device& device = state.devices[deviceIndex];
                device.vkDeviceIndex = deviceIndex;
                device.physical = physicalDevices[deviceIndex];
            }
        }

        // logical devices
        {
            VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            float priority = 0.0;
            queueInfo.pQueuePriorities = &priority;
            queueInfo.queueCount = 1;
            VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            for (uint32_t deviceIndex = 0; deviceIndex < state.devices.size(); ++deviceIndex)
            {
                State::Device& device = state.devices[deviceIndex];
                const uint32_t queueFamilyIndex = GetGraphicsOrComputeQueueFamilyIndex(device.physical);
                if (queueFamilyIndex != uint32_t(~0))
                {
                    queueInfo.queueFamilyIndex = queueFamilyIndex;
                    deviceCreateInfo.queueCreateInfoCount = 1;
                    deviceCreateInfo.pQueueCreateInfos = &queueInfo;
                }

                std::vector<const char*> requiredDeviceExtensionNames;
                bool success = VulkanAppendDeviceRequiredExtensions(state.instance, device.physical, (void*)vkGetInstanceProcAddr, requiredDeviceExtensionNames);
                if (!success)
                {
                    NV_PERF_LOG_ERR(50, "VulkanAppendDeviceRequiredExtensions failed!\n");
                }
                deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensionNames.size());
                deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensionNames.data();
                vulkanStatus = vkCreateDevice(device.physical, &deviceCreateInfo, nullptr, &device.logical);
                if (vulkanStatus != VK_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "vkCreateDevice failed for device index %u with profiler required extensions enabled!\n", deviceIndex);
                    // try to create a device without any required extensions enabled
                    deviceCreateInfo.enabledExtensionCount = 0;
                    deviceCreateInfo.ppEnabledExtensionNames = nullptr;
                    vulkanStatus = vkCreateDevice(device.physical, &deviceCreateInfo, nullptr, &device.logical);
                    if (vulkanStatus != VK_SUCCESS)
                    {
                        NV_PERF_LOG_ERR(50, "vkCreateDevice failed for device index %u without any profiler required extensions enabled!\n", deviceIndex);
                        return false;
                    }
                }

                if (queueFamilyIndex != uint32_t(~0))
                {
                    vkGetDeviceQueue(device.logical, queueFamilyIndex, 0, &device.queue);
                }
            }
        }

        // profiler-specific
        {
            bool nvperfStatus = true;
            nvperfStatus = VulkanLoadDriver(state.instance);
            if (!nvperfStatus)
            {
                NV_PERF_LOG_ERR(10, "VulkanLoadDriver failed!\n");
            }
            else
            {
                state.isDriverLoaded = true;
                for (uint32_t deviceIndex = 0; deviceIndex < state.devices.size(); ++deviceIndex)
                {
                    State::Device& device = state.devices[deviceIndex];
                    if (VulkanIsNvidiaDevice(device.physical))
                    {
                        device.nvpwDeviceIndex = VulkanGetNvperfDeviceIndex(state.instance, device.physical, device.logical);
                        if (device.nvpwDeviceIndex == ~size_t(0))
                        {
                            NV_PERF_LOG_ERR(50, "VulkanGetNvperfDeviceIndex failed for device index %u!\n", deviceIndex);
                        }
                    }
                }
            }
        }

        return true;
    }

    inline void AppendInstanceState(ordered_json& node)
    {
        // layers
        {
            node["AvailableInstanceLayers"] = ordered_json::array(); // we only have instance layers, "device layers" are now deprecated
            std::vector<VkLayerProperties> properties;
            bool success = GetAvailableInstanceLayerProperties(properties);
            if (success)
            {
                auto& layers = node["AvailableInstanceLayers"];
                for (const VkLayerProperties& vkLayerProperty : properties)
                {
                    auto property = ordered_json();
                    property["Name"] = vkLayerProperty.layerName;
                    property["Description"] = vkLayerProperty.description;
                    property["SpecVersion"] = vkLayerProperty.specVersion;
                    property["ImplementationVersion"] = vkLayerProperty.implementationVersion;
                    layers.emplace_back(std::move(property));
                }
            }
        }
        // TODO: any easy way to retrieve the list of "implicit layers" enabled?
        // profiler required instance extensions
        {
            node["ProfilerRequiredInstanceExtensionsSupported"] = nullptr;
            std::map<const char*, bool> requiredInstanceExtensionSupportStatus;
            if (GetRequiredInstanceExtensionSupportStatus(VulkanGetInstanceApiVersion(), requiredInstanceExtensionSupportStatus))
            {
                node["ProfilerRequiredInstanceExtensionsSupported"] = requiredInstanceExtensionSupportStatus;
            }
        }
    }

    inline void AppendDeviceState(const State& state, size_t deviceIndex, ordered_json& node)
    {
        const State::Device& currentDevice = state.devices[deviceIndex];
        VkPhysicalDevice physicalDevice = currentDevice.physical;
        VkDevice logicalDevice = currentDevice.logical;
        node["VKDeviceIndex"] = deviceIndex;
        bool isNvidiaDevice = false;

        // physical properties
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            node["Name"] = properties.deviceName;
            node["Type"] = ToCString(properties.deviceType);
            node["VendorId"] = properties.vendorID;
            isNvidiaDevice = VulkanIsNvidiaDevice(physicalDevice);
            node["DeviceId"] = properties.deviceID;
            node["ApiVersion"] = GetApiVersion(properties);
        }

        // device id properties
        {
            const VkPhysicalDeviceIDProperties idProperties = GetDeviceIdProperties(physicalDevice);
            node["DeviceUUID"] = GetDeviceUUID(idProperties);
            node["DeviceLUID"] = GetDeviceLUID(idProperties);
            node["DeviceNodeMask"] = nullptr;
            uint32_t nodeMask = 0;
            if (GetDeviceNodeMask(idProperties, nodeMask))
            {
                node["DeviceNodeMask"] = nodeMask;
            }
        }

        // NV-specific properties
        node["IsNvidiaDevice"] = isNvidiaDevice;
        node["ProfilerDeviceIndex"] = nullptr;
        node["ProfilerIsGpuSupported"]["IsSupported"] = false;
        node["ProfilerIsGpuSupported"]["GpuArchitectureSupported"] = nullptr;
        node["ProfilerIsGpuSupported"]["SliSupportLevel"] = nullptr;
        node["ProfilerIsGpuSupported"]["CmpSupportLevel"] = nullptr;
        node["ProfilerIsGpuSupported"]["Advice"] = "Unrecognized device";
        node["ProfilerIsSessionSupported"]["IsSupported"] = false;
        node["ProfilerIsSessionSupported"]["Advice"] = "Unsupported Gpu";
        node["ProfilerRequiredDeviceExtensionsSupported"] = nullptr;
        [&]() {
            if (!isNvidiaDevice || currentDevice.nvpwDeviceIndex == size_t(~0))
            {
                return false;
            }
            node["ProfilerDeviceIndex"] = currentDevice.nvpwDeviceIndex;

            NVPW_VK_Profiler_IsGpuSupported_Params isGpuSupportedParams = { NVPW_VK_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
            isGpuSupportedParams.deviceIndex = currentDevice.nvpwDeviceIndex;
            NVPA_Status nvpaStatus = NVPW_VK_Profiler_IsGpuSupported(&isGpuSupportedParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_IsGpuSupported failed on %s\n", VulkanGetDeviceName(physicalDevice).c_str());
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

            // profiler required device extensions
            std::map<const char*, bool> requiredDeviceExtensionSupportStatus;
            if (GetRequiredDeviceExtensionSupportStatus(state.instance, physicalDevice, requiredDeviceExtensionSupportStatus))
            {
                node["ProfilerRequiredDeviceExtensionsSupported"] = requiredDeviceExtensionSupportStatus;
            }

            // test if we can start a profiler session on this device
            nvpaStatus = ProfilerSessionSupported(state.instance, physicalDevice, logicalDevice, currentDevice.queue);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(10, "ProfilerSessionSupported failed on %s\n", VulkanGetDeviceName(physicalDevice).c_str());
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
        // append instance state
        AppendInstanceState(node);

        // append other global info
        node["ProfilerDriverLoaded"] = state.isDriverLoaded;

        // append per-device state
        auto deviceArray = ordered_json::array();
        for (size_t deviceIndex = 0; deviceIndex < state.devices.size(); ++deviceIndex)
        {
            auto device = ordered_json();
            AppendDeviceState(state, deviceIndex, device);
            deviceArray.emplace_back(std::move(device));
        }
        node["Devices"] = deviceArray;
    }

    inline void CleanupState(State& state)
    {
        for (State::Device& device : state.devices)
        {
            vkDestroyDevice(device.logical, nullptr);
        }
        vkDestroyInstance(state.instance, nullptr);
        state = State();
    }

}}}} // nv::perf::tool::vk

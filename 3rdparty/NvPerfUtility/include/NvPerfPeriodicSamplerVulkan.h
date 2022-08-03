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

#include <cstdint>
#include "NvPerfInit.h"
#include "NvPerfCounterConfiguration.h"
#include "NvPerfCounterData.h"
#include "NvPerfDeviceProperties.h"
#include "NvPerfMiniTraceVulkan.h"
#include "NvPerfPeriodicSamplerGpu.h"
#include "NvPerfVulkan.h"

namespace nv { namespace perf { namespace sampler {

    class PeriodicSamplerTimeHistoryVulkan
    {
    private:
        enum class SamplerStatus
        {
            Uninitialized,
            Failed,
            InitializedButNotInSession,
            InSession,
        };

        GpuPeriodicSampler m_periodicSamplerGpu;
        RingBufferCounterData m_counterData;
        mini_trace::MiniTracerVulkan m_tracer;
        uint32_t m_maxTriggerLatency;
        bool m_isFirstFrame;
        SamplerStatus m_status;

    public:
        struct FrameDelimiter
        {
            uint64_t frameEndTime;
        };

    public:
        PeriodicSamplerTimeHistoryVulkan()
            : m_maxTriggerLatency()
            , m_isFirstFrame(true)
            , m_status(SamplerStatus::Uninitialized)
        {
        }
        PeriodicSamplerTimeHistoryVulkan(const PeriodicSamplerTimeHistoryVulkan& sampler) = delete;
        PeriodicSamplerTimeHistoryVulkan(PeriodicSamplerTimeHistoryVulkan&& sampler)
            : m_periodicSamplerGpu(std::move(sampler.m_periodicSamplerGpu))
            , m_counterData(std::move(sampler.m_counterData))
            , m_tracer(std::move(sampler.m_tracer))
            , m_maxTriggerLatency(sampler.m_maxTriggerLatency)
            , m_isFirstFrame(sampler.m_isFirstFrame)
            , m_status(sampler.m_status)
        {
            sampler.m_status = SamplerStatus::Uninitialized;
        }
        ~PeriodicSamplerTimeHistoryVulkan()
        {
            Reset();
        }
        PeriodicSamplerTimeHistoryVulkan& operator=(const PeriodicSamplerTimeHistoryVulkan& sampler) = delete;
        PeriodicSamplerTimeHistoryVulkan& operator=(PeriodicSamplerTimeHistoryVulkan&& sampler)
        {
            Reset();
            m_periodicSamplerGpu = std::move(sampler.m_periodicSamplerGpu);
            m_counterData = std::move(sampler.m_counterData);
            m_tracer = std::move(sampler.m_tracer);
            m_maxTriggerLatency = sampler.m_maxTriggerLatency;
            m_isFirstFrame = sampler.m_isFirstFrame;
            m_status = sampler.m_status;
            sampler.m_status = SamplerStatus::Uninitialized;
            return *this;
        }

        static void AppendDeviceRequiredExtensions(uint32_t vulkanApiVersion, std::vector<const char*>& deviceExtensionNames)
        {
            mini_trace::MiniTracerVulkan::AppendDeviceRequiredExtensions(vulkanApiVersion, deviceExtensionNames);
        }

        bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
        {
            if (m_status != SamplerStatus::Uninitialized)
            {
                NV_PERF_LOG_ERR(50, "Not in an initializable state\n");
                return false;
            }
            auto setStatusToFailed = ScopeExitGuard([&]() {
                m_status = SamplerStatus::Failed;
            });
            if (!InitializeNvPerf())
            {
                NV_PERF_LOG_ERR(10, "InitializeNvPerf failed\n");
                return false;
            }

            if (!VulkanLoadDriver(instance))
            {
                NV_PERF_LOG_ERR(10, "Could not load driver\n");
                return false;
            }

            const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device);
            if (deviceIndex == (size_t)~0)
            {
                NV_PERF_LOG_ERR(20, "VulkanGetNvperfDeviceIndex failed\n");
                return false;
            }

            bool success = m_periodicSamplerGpu.Initialize(deviceIndex);
            if (!success)
            {
                return false;
            }
            success = m_tracer.Initialize(instance, physicalDevice, device);
            if (!success)
            {
                return false;
            }
            m_isFirstFrame = true;
            setStatusToFailed.Dismiss();
            m_status = SamplerStatus::InitializedButNotInSession;
            return true;
        }

        void Reset()
        {
            m_periodicSamplerGpu.Reset();
            m_counterData.Reset();
            m_tracer.Reset();
            m_maxTriggerLatency = 0;
            m_isFirstFrame = true;
            m_status = SamplerStatus::Uninitialized;
        }

        bool IsInitialized() const
        {
            return (m_status == SamplerStatus::InitializedButNotInSession) || (m_status == SamplerStatus::InSession);
        }

        size_t GetGpuDeviceIndex() const
        {
            return m_periodicSamplerGpu.GetDeviceIndex();
        }

        const DeviceIdentifiers& GetGpuDeviceIdentifiers() const
        {
            return m_periodicSamplerGpu.GetDeviceIdentifiers();
        }

        const std::vector<uint8_t>& GetCounterData() const
        {
            return m_counterData.GetCounterData();
        }

        std::vector<uint8_t>& GetCounterData()
        {
            return m_counterData.GetCounterData();
        }

        bool BeginSession(VkQueue queue, uint32_t queueFamilyIndex, uint32_t samplingIntervalInNanoSeconds, uint32_t maxDecodeLatencyInNanoSeconds, size_t maxFrameLatency)
        {
            if (m_status != SamplerStatus::InitializedButNotInSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a state ready for BeginSession\n");
                return false;
            }
            auto setStatusToFailed = ScopeExitGuard([&]() {
                m_status = SamplerStatus::Failed;
            });
            const GpuPeriodicSampler::GpuPulseSamplingInterval samplingInterval = m_periodicSamplerGpu.GetGpuPulseSamplingInterval(samplingIntervalInNanoSeconds);
            m_maxTriggerLatency = maxDecodeLatencyInNanoSeconds / samplingIntervalInNanoSeconds;
            size_t recordBufferSize = 0;
            bool success = GpuPeriodicSamplerCalculateRecordBufferSize(GetGpuDeviceIndex(), std::vector<uint8_t>(), m_maxTriggerLatency, recordBufferSize);
            if (!success)
            {
                return false;
            }
            const size_t maxNumUndecodedSamplingRanges = 1;
            success = m_periodicSamplerGpu.BeginSession(recordBufferSize, maxNumUndecodedSamplingRanges, { samplingInterval.triggerSource }, samplingInterval.samplingInterval);
            if (!success)
            {
                return false;
            }
            success = m_tracer.BeginSession(queue, queueFamilyIndex, maxFrameLatency);
            if (!success)
            {
                return false;
            }
            setStatusToFailed.Dismiss();
            m_status = SamplerStatus::InSession;
            return true;
        }
        
        bool EndSession()
        {
            bool success = m_periodicSamplerGpu.EndSession();
            if (!success)
            {
                // continue
            }
            m_tracer.EndSession();
            m_maxTriggerLatency = 0;
            if (m_status == SamplerStatus::InSession)
            {
                m_status = SamplerStatus::InitializedButNotInSession;
            }
            return success;
        }

        bool SetConfig(const CounterConfiguration* pCounterConfiguration)
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            if (!pCounterConfiguration || pCounterConfiguration->numPasses != 1)
            {
                NV_PERF_LOG_ERR(30, "Invalid counter config\n");
                return false;
            }

            const size_t passIndex = 0;
            bool success = m_periodicSamplerGpu.SetConfig(pCounterConfiguration->configImage, passIndex);
            if (!success)
            {
                return false;
            }

            m_counterData.Reset();
            const bool validate = false; // set this to true to perform extra validation, useful for debugging
            success = m_counterData.Initialize(m_maxTriggerLatency, validate, [&](uint32_t maxSamples, NVPW_PeriodicSampler_CounterData_AppendMode appendMode, std::vector<uint8_t>& counterData) {
                return GpuPeriodicSamplerCreateCounterData(
                    GetGpuDeviceIndex(),
                    pCounterConfiguration->counterDataPrefix.data(),
                    pCounterConfiguration->counterDataPrefix.size(),
                    maxSamples,
                    appendMode,
                    counterData);
            });
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool StartSampling()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            bool success = m_periodicSamplerGpu.StartSampling();
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool StopSampling()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            bool success = m_periodicSamplerGpu.StopSampling();
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool OnFrameEnd()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            bool success = true;
            if (m_isFirstFrame)
            {
                success = StartSampling();
                if (!success)
                {
                    return false;
                }
                m_isFirstFrame = false;
            }
            success = m_tracer.OnFrameEnd();
            if (!success)
            {
                return false;
            }
            return true;
        }

        std::vector<FrameDelimiter> GetFrameDelimiters()
        {
            std::vector<FrameDelimiter> delimiters;
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return delimiters;
            }
            while (true)
            {
                bool isDataReady = false;
                mini_trace::MiniTracerVulkan::FrameData frameData{};
                if (!m_tracer.GetOldestFrameData(isDataReady, frameData))
                {
                    break;
                }
                if (!isDataReady)
                {
                    break;
                }
                delimiters.push_back(FrameDelimiter{ frameData.frameEndTime });
                if (!m_tracer.ReleaseOldestFrame())
                {
                    break;
                }
            }
            return delimiters;
        }

        bool DecodeCounters()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            const size_t numSamplingRangesToDecode = 1;
            size_t numSamplingRangesDecoded = 0;
            bool recordBufferOverflow = false;
            size_t numSamplesDropped = 0;
            size_t numSamplesMerged = 0;
            bool success = m_periodicSamplerGpu.DecodeCounters(
                m_counterData.GetCounterData(),
                numSamplingRangesToDecode,
                numSamplingRangesDecoded,
                recordBufferOverflow,
                numSamplesDropped,
                numSamplesMerged);
            if (!success)
            {
                return false;
            }
            if (recordBufferOverflow)
            {
                NV_PERF_LOG_ERR(20, "Record buffer overflow has been detected! Please try to 1) reduce sampling frequency 2) increase record buffer size "
"3) call DecodeCounters() more frequently\n");
                return false;
            }
            if (numSamplesMerged)
            {
                NV_PERF_LOG_WRN(100, "Merged samples have been detected! This may lead to reduced accuracy. please try to reduce the sampling frequency.\n");
            }

            success = m_counterData.UpdatePut();
            if (!success)
            {
                return false;
            }
            return true;
        }

        // TConsumeRangeDataFunc should be in the form of bool(const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop),
        // return false to indicate something went wrong; set "stop" to true to early break from iterating succeeding unread ranges, "this" range will not be recycled
        template <typename TConsumeRangeDataFunc>
        bool ConsumeSamples(TConsumeRangeDataFunc&& consumeRangeDataFunc)
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            uint32_t numRangesConsumed = 0;
            bool success = m_counterData.ConsumeData([&, userFunc = std::forward<TConsumeRangeDataFunc>(consumeRangeDataFunc)](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                bool userFuncSuccess = userFunc(pCounterDataImage, counterDataImageSize, rangeIndex, stop);
                if (!userFuncSuccess)
                {
                    return false;
                }
                if (!stop)
                {
                    ++numRangesConsumed;
                }
                return true; // simply pass "stop" along, m_counterData.ConsumeData will further process it and early break
            });
            if (!success)
            {
                return false;
            }

            success = m_counterData.UpdateGet(numRangesConsumed);
            if (!success)
            {
                return false;
            }
            return true;
        }
    };

}}}
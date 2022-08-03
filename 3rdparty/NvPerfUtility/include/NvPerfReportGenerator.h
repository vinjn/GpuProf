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

#include <math.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif
#include "NvPerfInit.h"
#include "NvPerfDeviceProperties.h"
#include "NvPerfMetricsEvaluator.h"
#include "NvPerfRangeProfiler.h"
#include "NvPerfReportDefinition.h"
#include "NvPerfReportDefinitionHAL.h"
#include "NvPerfCommonHtmlTemplates.h"

#ifdef WIN32
#define NV_PERF_PATH_SEPARATOR '\\'
#else
#define NV_PERF_PATH_SEPARATOR '/'
#endif

namespace nv { namespace perf {

    enum class AppendDateTime
    {
        no,
        yes
    };

    struct BaseMetricRequests
    {
        struct Request
        {
            size_t metricIndex;
        };
        std::vector<Request> requests[NVPW_METRIC_TYPE__COUNT]; // one for each metric type
    };

    struct SubmetricRequests
    {
        struct Request
        {
            NVPW_RollupOp rollupOp;
            NVPW_Submetric submetric;
        };
        std::vector<Request> requests[NVPW_METRIC_TYPE__COUNT]; // one for each metric type
    };

    struct ReportLayout
    {
        std::string gpuName;
        std::string chipName;

        struct Summary
        {
            ReportDefinition definition;
            BaseMetricRequests baseMetricRequests;
            SubmetricRequests submetricRequests;
        } summary;

        struct PerRange
        {
            ReportDefinition definition;
            BaseMetricRequests baseMetricRequests;
            SubmetricRequests submetricRequests;
        } perRange;
    };

    struct ReportData
    {
        struct RangeData
        {
            std::string fullName;                     ///< e.g. Frame/Draw
            std::string leafName;                     ///< e.g. Draw
            std::vector<double> summaryReportValues;  ///< Product of metric_type x base x sub
            std::vector<double> perRangeReportValues; ///< Product of metric_type x base x sub
        };

        std::string reportDirectoryName;
        uint64_t secondsSinceEpoch;
        NVPW_Device_ClockStatus clockStatus;
        const uint8_t* pCounterDataImage;
        size_t counterDataImageSize;
        std::vector<RangeData> ranges;
    };

    typedef std::function<void(const MetricsEvaluator&, const ReportLayout&, const ReportData&)> ReportWriterFn;

    struct ReportOutputOptions
    {
        bool enableHtmlReport = true;
        bool enableCsvReport = true;
        std::vector<ReportWriterFn> reportWriters;  ///< User-defined report writers
    };

    inline size_t GetTotalNumSubmetrics(const BaseMetricRequests& baseMetricRequests, const SubmetricRequests& submetricRequests)
    {
        size_t totalNumSubmetrics = 0;
        for (size_t metricType = 0; metricType < NVPW_METRIC_TYPE__COUNT; ++metricType)
        {
            totalNumSubmetrics += baseMetricRequests.requests[metricType].size() * submetricRequests.requests[metricType].size();
        }
        return totalNumSubmetrics;
    }

    inline void GetExpandedMetricEvalRequestList(const BaseMetricRequests& baseMetricRequests, const SubmetricRequests& submetricRequests, std::vector<NVPW_MetricEvalRequest>& metricEvalRequests)
    {
        const size_t totalNumSubmetrics = GetTotalNumSubmetrics(baseMetricRequests, submetricRequests);
        metricEvalRequests.reserve(totalNumSubmetrics);
        for (size_t metricType = 0; metricType < NVPW_METRIC_TYPE__COUNT; ++metricType)
        {
            const auto& perTypeBaseMetrics = baseMetricRequests.requests[metricType];
            const auto& perTypeSubmetrics = submetricRequests.requests[metricType];

            NVPW_MetricEvalRequest metricEvalRequest{};
            metricEvalRequest.metricType = static_cast<uint8_t>(metricType);
            for (const BaseMetricRequests::Request& baseMetric : perTypeBaseMetrics)
            {
                metricEvalRequest.metricIndex = baseMetric.metricIndex;
                for (const SubmetricRequests::Request& submetric : perTypeSubmetrics)
                {
                    metricEvalRequest.rollupOp = static_cast<uint8_t>(submetric.rollupOp);
                    metricEvalRequest.submetric = static_cast<uint16_t>(submetric.submetric);
                    metricEvalRequests.push_back(metricEvalRequest);
                }
            }
        }
    }

    // iterate and perform func on metrics with a fixed-sized buffer, all submetrics that belong to the same base metric are performed in a batch for better perf
    template <class TFunc>
    inline bool ForEachBaseMetric(const BaseMetricRequests& baseMetricRequests, const SubmetricRequests& submetricRequests, TFunc&& func)
    {
        NVPW_MetricEvalRequest metricEvalRequests[16] = {};
        for (size_t metricType = 0; metricType < NVPW_METRIC_TYPE__COUNT; ++metricType)
        {
            // all metrics of the same type share a common list of submetrics, so we only need to fill them once
            const auto& perTypeSubmetrics = submetricRequests.requests[metricType];
            const size_t numSubmetrics = perTypeSubmetrics.size();
            assert(numSubmetrics <= sizeof(metricEvalRequests) / sizeof(metricEvalRequests[0]));
            for (size_t submetricIdx = 0; submetricIdx < numSubmetrics; ++submetricIdx)
            {
                const SubmetricRequests::Request& submetric = perTypeSubmetrics[submetricIdx];
                NVPW_MetricEvalRequest& metricEvalRequest = metricEvalRequests[submetricIdx];
                metricEvalRequest.metricType = static_cast<uint8_t>(metricType);
                metricEvalRequest.rollupOp = static_cast<uint8_t>(submetric.rollupOp);
                metricEvalRequest.submetric = static_cast<uint16_t>(submetric.submetric);
            }

            const auto& perTypeBaseMetrics = baseMetricRequests.requests[metricType];
            for (const BaseMetricRequests::Request& baseMetric : perTypeBaseMetrics)
            {
                // for each base metric, populate its metric index to those metricEvalRequests
                std::for_each(std::begin(metricEvalRequests), std::begin(metricEvalRequests) + numSubmetrics, [&](NVPW_MetricEvalRequest& metricEvalRequest) {
                    metricEvalRequest.metricIndex = baseMetric.metricIndex;
                });
                if (!func(metricEvalRequests, numSubmetrics))
                {
                    return false;
                }
            }
        }
        return true;
    }

    inline std::string FormatJsDouble(double value)
    {
        if (isnan(value))
        {
            return "\"NaN\"";
        }
        if (isinf(value))
        {
            if (value < 0)
            {
                return "\"-Infinity\"";
            }
            return "\"Infinity\"";
        }
        char buf[128];
#ifdef WIN32
        sprintf_s(buf, "%f", value);
#else
        snprintf(buf, 128, "%f", value);
#endif
        return buf;
    }

    inline std::string MakeReport(const ReportDefinition& reportDefinition, const std::string& jsonContents)
    {
        const char* pJsonReplacementMarker = "/***JSON_DATA_HERE***/";
        const char* pInsertPoint = strstr(reportDefinition.pReportHtml, pJsonReplacementMarker);
        if (!pInsertPoint)
        {
            return "";
        }

        std::string reportHtml;
        reportHtml.append(reportDefinition.pReportHtml, pInsertPoint - reportDefinition.pReportHtml);
        reportHtml.append(jsonContents);
        reportHtml.append(pInsertPoint + strlen(pJsonReplacementMarker));

        return reportHtml;
    }

    inline std::string FormatTime(const time_t& secondsSinceEpoch)
    {
        struct tm now_time;
#ifdef WIN32
        const errno_t ret = localtime_s(&now_time, &secondsSinceEpoch);
        if (ret)
        {
            return std::string();
        }
#else
        const tm* pRet = localtime_r(&secondsSinceEpoch, &now_time);
        if (!pRet)
        {
            return std::string();
        }
#endif
        std::stringstream sstream;
        sstream << std::setfill('0') << 1900 + now_time.tm_year << std::setw(2) << 1 + now_time.tm_mon << std::setw(2) << now_time.tm_mday << "_"
            << std::setw(2) << now_time.tm_hour << std::setw(2) << now_time.tm_min << std::setw(2) << now_time.tm_sec;
        return sstream.str();
    }

    inline std::string GetRangeFileName(size_t rangeIndex, const char* pLeafName)
    {
        std::string leafName(pLeafName);
        for (char& c : leafName)
        {
            if (!std::isalnum(c) && c != '_' && c != '-' && c != '.')
            {
                c = '_';
            }
        }
        std::stringstream sstream;
        sstream << std::setfill('0') << std::setw(5) << rangeIndex << "_" << leafName << ".html";
        return sstream.str();
    }


    namespace PerRangeReport {

        inline void InitReportDataMetrics(NVPW_MetricsEvaluator* pMetricsEvaluator, const std::vector<std::string>& additionalMetrics, ReportLayout& reportLayout)
        {
            bool success = true;
            reportLayout.perRange.baseMetricRequests = {};
            reportLayout.perRange.submetricRequests = {};

            // submetrics
            {
                const SubmetricRequests::Request CounterSubmetrics[] = {
                    { NVPW_ROLLUP_OP_SUM, NVPW_SUBMETRIC_NONE                           }, // "sum"
                    { NVPW_ROLLUP_OP_SUM, NVPW_SUBMETRIC_PEAK_SUSTAINED                 }, // "sum.peak_sustained"
                    { NVPW_ROLLUP_OP_SUM, NVPW_SUBMETRIC_PER_SECOND                     }, // "sum.per_second"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_NONE                           }, // "avg"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PER_CYCLE_ELAPSED              }, // "avg.per_cycle_elapsed"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PER_SECOND                     }, // "avg.per_second"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ELAPSED  }, // "avg.pct_of_peak_sustained_elapsed"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PEAK_SUSTAINED                 }, // "avg.peak_sustained"
                };

                const SubmetricRequests::Request RatioSubmetrics[] = {
                    // rollups don't matter to ratios
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PCT                            }, // "pct"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_RATIO                          }, // "ratio"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_MAX_RATE                       }, // "max_rate"
                };

                const SubmetricRequests::Request ThroughputSubmetrics[] = {
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ELAPSED  }, // "avg.pct_of_peak_sustained_elapsed"
                };

                auto& counterSubmetrics = reportLayout.perRange.submetricRequests.requests[NVPW_METRIC_TYPE_COUNTER];
                counterSubmetrics.insert(std::end(counterSubmetrics), std::begin(CounterSubmetrics), std::end(CounterSubmetrics));

                auto& ratioSubmetrics = reportLayout.perRange.submetricRequests.requests[NVPW_METRIC_TYPE_RATIO];
                ratioSubmetrics.insert(std::end(ratioSubmetrics), std::begin(RatioSubmetrics), std::end(RatioSubmetrics));

                auto& throughputSubmetrics = reportLayout.perRange.submetricRequests.requests[NVPW_METRIC_TYPE_THROUGHPUT];
                throughputSubmetrics.insert(std::end(throughputSubmetrics), std::begin(ThroughputSubmetrics), std::end(ThroughputSubmetrics));
            }

            // metrics
            {
                // counters
                auto& counters = reportLayout.perRange.baseMetricRequests.requests[NVPW_METRIC_TYPE_COUNTER];
                for (size_t counterIdx = 0; counterIdx < reportLayout.perRange.definition.numCounters; ++counterIdx)
                {
                    const char* pCounterName = reportLayout.perRange.definition.ppCounterNames[counterIdx];
                    NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
                    size_t metricIndex = ~size_t(0);
                    success = GetMetricTypeAndIndex(pMetricsEvaluator, pCounterName, metricType, metricIndex);
                    if (!success || (metricType != NVPW_METRIC_TYPE_COUNTER) || (metricIndex == ~size_t(0)))
                    {
                        NV_PERF_LOG_WRN(50, "GetMetricTypeAndIndex failed for metric: %s\n", pCounterName);
                        continue;
                    }
                    counters.emplace_back(BaseMetricRequests::Request{metricIndex});
                }

                // ratios
                auto& ratios = reportLayout.perRange.baseMetricRequests.requests[NVPW_METRIC_TYPE_RATIO];
                for (size_t ratioIdx = 0; ratioIdx < reportLayout.perRange.definition.numRatios; ++ratioIdx)
                {
                    const char* pRatioName = reportLayout.perRange.definition.ppRatioNames[ratioIdx];
                    NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
                    size_t metricIndex = ~size_t(0);
                    success = GetMetricTypeAndIndex(pMetricsEvaluator, pRatioName, metricType, metricIndex);
                    if (!success || (metricType != NVPW_METRIC_TYPE_RATIO) || (metricIndex == ~size_t(0)))
                    {
                        NV_PERF_LOG_WRN(50, "GetMetricTypeAndIndex failed for metric: %s\n", pRatioName);
                        continue;
                    }
                    ratios.emplace_back(BaseMetricRequests::Request{metricIndex});
                }

                // throughputs
                auto& throughputs = reportLayout.perRange.baseMetricRequests.requests[NVPW_METRIC_TYPE_THROUGHPUT];
                for (size_t throughputIdx = 0; throughputIdx < reportLayout.perRange.definition.numThroughputs; ++throughputIdx)
                {
                    const char* pThroughputName = reportLayout.perRange.definition.ppThroughputNames[throughputIdx];
                    NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
                    size_t metricIndex = ~size_t(0);
                    success = GetMetricTypeAndIndex(pMetricsEvaluator, pThroughputName, metricType, metricIndex);
                    if (!success || (metricType != NVPW_METRIC_TYPE_THROUGHPUT) || (metricIndex == ~size_t(0)))
                    {
                        NV_PERF_LOG_WRN(50, "GetMetricTypeAndIndex failed for metric: %s\n", pThroughputName);
                        continue;
                    }
                    throughputs.emplace_back(BaseMetricRequests::Request{metricIndex});
                }

                // additional metrics
                for (size_t additionalMetricIdx = 0; additionalMetricIdx < additionalMetrics.size(); ++additionalMetricIdx)
                {
                    const char* pMetricName = additionalMetrics[additionalMetricIdx].c_str();
                    NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
                    size_t metricIndex = ~size_t(0);
                    success = GetMetricTypeAndIndex(pMetricsEvaluator, pMetricName, metricType, metricIndex);
                    if (!success || (metricType == NVPW_METRIC_TYPE__COUNT) || (metricIndex == ~size_t(0)))
                    {
                        NV_PERF_LOG_WRN(50, "GetMetricTypeAndIndex failed for metric: %s\n", pMetricName);
                        continue;
                    }
                    if (metricType == NVPW_METRIC_TYPE_COUNTER)
                    {
                        counters.emplace_back(BaseMetricRequests::Request{metricIndex});
                    }
                    else if (metricType == NVPW_METRIC_TYPE_RATIO)
                    {
                        ratios.emplace_back(BaseMetricRequests::Request{metricIndex});
                    }
                    else if (metricType == NVPW_METRIC_TYPE_THROUGHPUT)
                    {
                        throughputs.emplace_back(BaseMetricRequests::Request{metricIndex});
                    }
                    else
                    {
                        NV_PERF_LOG_WRN(50, "Unrecognized metric type for metric: %s\n", pMetricName);
                    }
                }
            }
        }

        // outputs key-value pairs for the report JSON, not including the enclosing brackets
        inline std::string MakeJsonContents(NVPW_MetricsEvaluator* pMetricsEvaluator, const ReportLayout& reportLayout, const ReportData& reportData, size_t rangeIndex)
        {
            const ReportData::RangeData& rangeData = reportData.ranges[rangeIndex];
            size_t metricIndex = 0; // incremented in the same deterministic pattern as construction

            std::stringstream sstream;
            sstream << "\"rangeName\": \"" << rangeData.fullName << "\",\n";
            sstream << "\"debug\": false,\n";
            sstream << "\"populateDummyValues\": false,\n";
            sstream << "\"secondsSinceEpoch\": " << reportData.secondsSinceEpoch << ",\n";
            sstream << "\"device\": {\n";
            sstream << "  \"gpuName\": \"" << reportLayout.gpuName << "\",\n";
            sstream << "  \"chipName\": \"" << reportLayout.chipName << "\",\n";
            sstream << "  \"clockLockingStatus\": \"" << ToCString(reportData.clockStatus) << "\"\n";
            sstream << "},\n";

            const MetricsEnumerator countersEnumerator = EnumerateCounters(pMetricsEvaluator);
            const MetricsEnumerator ratiosEnumerator = EnumerateRatios(pMetricsEvaluator);
            const MetricsEnumerator throughputsEnumerator = EnumerateThroughputs(pMetricsEvaluator);
            for (size_t metricType = 0; metricType < NVPW_METRIC_TYPE__COUNT; ++metricType)
            {
                if (metricType)
                {
                    sstream << ", ";
                }

                if (metricType == NVPW_METRIC_TYPE_COUNTER)
                {
                    sstream << "\"counters\": {\n";
                }
                else if (metricType == NVPW_METRIC_TYPE_RATIO)
                {
                    sstream << "\"ratios\": {\n";
                }
                else if (metricType == NVPW_METRIC_TYPE_THROUGHPUT)
                {
                    sstream << "\"throughputs\": {\n";
                }

                const auto& perTypeBaseMetrics = reportLayout.perRange.baseMetricRequests.requests[metricType];
                const auto& perTypeSubmetrics = reportLayout.perRange.submetricRequests.requests[metricType];
                for (size_t baseMetricIndex = 0; baseMetricIndex < perTypeBaseMetrics.size(); ++baseMetricIndex)
                {
                    const BaseMetricRequests::Request& baseMetric = perTypeBaseMetrics[baseMetricIndex];

                    if (baseMetricIndex)
                    {
                        sstream << ", ";
                    }

                    sstream << "\"" << ToCString(countersEnumerator, ratiosEnumerator, throughputsEnumerator, static_cast<NVPW_MetricType>(metricType), baseMetric.metricIndex) << "\": { ";
                    for (size_t submetricIndex = 0; submetricIndex < perTypeSubmetrics.size(); ++submetricIndex)
                    {
                        const SubmetricRequests::Request& submetric = perTypeSubmetrics[submetricIndex];

                        if (submetricIndex)
                        {
                            sstream << ", ";
                        }

                        std::string submetricName;
                        if (metricType == NVPW_METRIC_TYPE_COUNTER || metricType == NVPW_METRIC_TYPE_THROUGHPUT)
                        {
                            submetricName += ToCString(submetric.rollupOp);
                        }
                        submetricName += ToCString(submetric.submetric);
                        sstream << "\"" << submetricName.substr(1) << "\": " << FormatJsDouble(rangeData.perRangeReportValues[metricIndex++]);
                    }
                    // if the metric type is Counter, additionally include its dimensional units
                    if (metricType == NVPW_METRIC_TYPE_COUNTER)
                    {
                        std::vector<NVPW_DimUnitFactor> dimUnits;
                        std::string dimUnitsStr;
                        const NVPW_MetricEvalRequest metricRequest{ baseMetric.metricIndex, static_cast<uint8_t>(NVPW_METRIC_TYPE_COUNTER), static_cast<uint8_t>(NVPW_ROLLUP_OP_AVG), static_cast<uint16_t>(NVPW_SUBMETRIC_NONE) };
                        const bool success = GetMetricDimUnits(pMetricsEvaluator, metricRequest, dimUnits);
                        if (success)
                        {
                            dimUnitsStr = ToString(dimUnits, [&](NVPW_DimUnitName dimUnit, bool plural) {
                                return ToCString(pMetricsEvaluator, dimUnit, plural);
                            });
                        }
                        if (!perTypeSubmetrics.empty())
                        {
                            sstream << ", ";
                        }
                        sstream << "\"dim_units\": \"" << dimUnitsStr << "\"";
                    }
                    sstream << " }\n";
                }
                sstream << "}\n";
            }

            std::string jsonContents = sstream.str();
            return jsonContents;
        }

        inline void WriteHtmlReportFiles(NVPW_MetricsEvaluator* pMetricsEvaluator, const ReportLayout& reportLayout, const ReportData& reportData)
        {
            for (size_t rangeIndex = 0, numRanges = reportData.ranges.size(); rangeIndex < numRanges; rangeIndex++)
            {
                const char* pLeafName = reportData.ranges[rangeIndex].leafName.c_str();
                const std::string filename(reportData.reportDirectoryName + NV_PERF_PATH_SEPARATOR + GetRangeFileName(rangeIndex, pLeafName));
                if (FILE* pHTMLFile = OpenFile(&filename[0], "wb"))
                {
                    const std::string jsonContents = MakeJsonContents(pMetricsEvaluator, reportLayout, reportData, rangeIndex);
                    const std::string reportHtml = MakeReport(reportLayout.perRange.definition, jsonContents);
                    fputs(reportHtml.c_str(), pHTMLFile);
                    fclose(pHTMLFile);
                }
                else
                {
                    NV_PERF_LOG_ERR(20, "OpenFile failed for file: %s\n", &filename[0]);
                }
            }
        }

        inline void WriteCsvReportFile(NVPW_MetricsEvaluator* pMetricsEvaluator, const ReportLayout& reportLayout, const ReportData& reportData)
        {
            const std::string filename = reportData.reportDirectoryName + NV_PERF_PATH_SEPARATOR + "nvperf_metrics.csv";
            FILE* fp = OpenFile(filename.c_str(), "wt");
            if (!fp)
            {
                NV_PERF_LOG_ERR(20, "OpenFile failed for file: %s\n", filename.c_str());
                return;
            }

            // print header
            const MetricsEnumerator countersEnumerator = EnumerateCounters(pMetricsEvaluator);
            const MetricsEnumerator ratiosEnumerator = EnumerateRatios(pMetricsEvaluator);
            const MetricsEnumerator throughputsEnumerator = EnumerateThroughputs(pMetricsEvaluator);
            fprintf(fp, "\"Range Name\",");
            ForEachBaseMetric(reportLayout.perRange.baseMetricRequests, reportLayout.perRange.submetricRequests, [&](const NVPW_MetricEvalRequest* pMetricEvalRequests, size_t numMetricEvalRequests) {
                for (size_t ii = 0; ii < numMetricEvalRequests; ++ii)
                {
                    fprintf(fp, "\"%s\",", ToString(countersEnumerator, ratiosEnumerator, throughputsEnumerator, pMetricEvalRequests[ii]).c_str());
                }
                return true;
            });
            fprintf(fp, "\n");

            // print ranges
            for (const ReportData::RangeData& rangeData : reportData.ranges)
            {
                fprintf(fp, "\"%s\",", rangeData.fullName.c_str());
                for (double metricValue : rangeData.perRangeReportValues)
                {
                    fprintf(fp, "%g, ", metricValue);
                }
                fprintf(fp, "\n");
            }

            fclose(fp);
        }

    } // namespace PerRangeReport

    namespace SummaryReport {

        inline void InitReportDataMetrics(NVPW_MetricsEvaluator* pMetricsEvaluator, ReportLayout& reportLayout)
        {
            bool success = true;
            reportLayout.summary.baseMetricRequests = {};
            reportLayout.summary.submetricRequests = {};

            // submetrics
            {
                const SubmetricRequests::Request CounterSubmetrics[] = {
                    { NVPW_ROLLUP_OP_SUM, NVPW_SUBMETRIC_NONE                           }, // "sum"
                    { NVPW_ROLLUP_OP_SUM, NVPW_SUBMETRIC_PEAK_SUSTAINED                 }, // "sum.peak_sustained"
                    { NVPW_ROLLUP_OP_SUM, NVPW_SUBMETRIC_PER_SECOND                     }, // "sum.per_second"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_NONE                           }, // "avg"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PER_CYCLE_ELAPSED              }, // "avg.per_cycle_elapsed"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PER_SECOND                     }, // "avg.per_second"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ELAPSED  }, // "avg.pct_of_peak_sustained_elapsed"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PEAK_SUSTAINED                 }, // "avg.peak_sustained"
                };

                const SubmetricRequests::Request RatioSubmetrics[] = {
                    // rollups don't matter to ratios
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PCT                            }, // "pct"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_RATIO                          }, // "ratio"
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_MAX_RATE                       }, // "max_rate"
                };

                const SubmetricRequests::Request ThroughputSubmetrics[] = {
                    { NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ELAPSED  }, // "avg.pct_of_peak_sustained_elapsed"
                };

                auto& counterSubmetrics = reportLayout.summary.submetricRequests.requests[NVPW_METRIC_TYPE_COUNTER];
                counterSubmetrics.insert(std::end(counterSubmetrics), std::begin(CounterSubmetrics), std::end(CounterSubmetrics));

                auto& ratioSubmetrics = reportLayout.summary.submetricRequests.requests[NVPW_METRIC_TYPE_RATIO];
                ratioSubmetrics.insert(std::end(ratioSubmetrics), std::begin(RatioSubmetrics), std::end(RatioSubmetrics));

                auto& throughputSubmetrics = reportLayout.summary.submetricRequests.requests[NVPW_METRIC_TYPE_THROUGHPUT];
                throughputSubmetrics.insert(std::end(throughputSubmetrics), std::begin(ThroughputSubmetrics), std::end(ThroughputSubmetrics));
            }

            // metrics
            {
                // counters
                auto& counters = reportLayout.summary.baseMetricRequests.requests[NVPW_METRIC_TYPE_COUNTER];
                for (size_t counterIdx = 0; counterIdx < reportLayout.summary.definition.numCounters; ++counterIdx)
                {
                    const char* pCounterName = reportLayout.summary.definition.ppCounterNames[counterIdx];
                    NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
                    size_t metricIndex = ~size_t(0);
                    success = GetMetricTypeAndIndex(pMetricsEvaluator, pCounterName, metricType, metricIndex);
                    if (!success || (metricType != NVPW_METRIC_TYPE_COUNTER) || (metricIndex == ~size_t(0)))
                    {
                        NV_PERF_LOG_WRN(50, "GetMetricTypeAndIndex failed for metric: %s\n", pCounterName);
                        continue;
                    }
                    counters.emplace_back(BaseMetricRequests::Request{metricIndex});
                }

                // ratios
                auto& ratios = reportLayout.summary.baseMetricRequests.requests[NVPW_METRIC_TYPE_RATIO];
                for (size_t ratioIdx = 0; ratioIdx < reportLayout.summary.definition.numRatios; ++ratioIdx)
                {
                    const char* pRatioName = reportLayout.summary.definition.ppRatioNames[ratioIdx];
                    NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
                    size_t metricIndex = ~size_t(0);
                    success = GetMetricTypeAndIndex(pMetricsEvaluator, pRatioName, metricType, metricIndex);
                    if (!success || (metricType != NVPW_METRIC_TYPE_RATIO) || (metricIndex == ~size_t(0)))
                    {
                        NV_PERF_LOG_WRN(50, "GetMetricTypeAndIndex failed for metric: %s\n", pRatioName);
                        continue;
                    }
                    ratios.emplace_back(BaseMetricRequests::Request{metricIndex});
                }

                // throughputs
                auto& throughputs = reportLayout.summary.baseMetricRequests.requests[NVPW_METRIC_TYPE_THROUGHPUT];
                for (size_t throughputIdx = 0; throughputIdx < reportLayout.summary.definition.numThroughputs; ++throughputIdx)
                {
                    const char* pThroughputName = reportLayout.summary.definition.ppThroughputNames[throughputIdx];
                    NVPW_MetricType metricType = NVPW_METRIC_TYPE__COUNT;
                    size_t metricIndex = ~size_t(0);
                    success = GetMetricTypeAndIndex(pMetricsEvaluator, pThroughputName, metricType, metricIndex);
                    if (!success || (metricType != NVPW_METRIC_TYPE_THROUGHPUT) || (metricIndex == ~size_t(0)))
                    {
                        NV_PERF_LOG_WRN(50, "GetMetricTypeAndIndex failed for metric: %s\n", pThroughputName);
                        continue;
                    }
                    throughputs.emplace_back(BaseMetricRequests::Request{metricIndex});
                }
            }
        }

        // outputs key-value pairs for the report JSON, not including the enclosing brackets
        inline std::string MakeJsonContents(NVPW_MetricsEvaluator* pMetricsEvaluator, const ReportLayout& reportLayout, const ReportData& reportData)
        {
            std::stringstream sstream;
            sstream << "\"debug\": false,\n";
            sstream << "\"populateDummyValues\": false,\n";
            sstream << "\"secondsSinceEpoch\": " << reportData.secondsSinceEpoch << ",\n";
            sstream << "\"device\": {\n";
            sstream << "  \"gpuName\": \"" << reportLayout.gpuName << "\",\n";
            sstream << "  \"chipName\": \"" << reportLayout.chipName << "\"\n";
            sstream << "},\n";
            sstream << "\"ranges\": [ ";
            for (size_t ii = 0; ii < reportData.ranges.size(); ++ii)
            {
                if (ii)
                {
                    sstream << ", ";
                }
                sstream << '"' << reportData.ranges[ii].fullName << '"';
            }
            sstream << "],\n";

            sstream << "\"range_file_names\": [ ";
            for (size_t ii = 0; ii < reportData.ranges.size(); ++ii)
            {
                if (ii)
                {
                    sstream << ", ";
                }
                sstream << '"' << GetRangeFileName(ii, reportData.ranges[ii].leafName.c_str()) << '"';
            }
            sstream << "],\n";

            const MetricsEnumerator countersEnumerator = EnumerateCounters(pMetricsEvaluator);
            const MetricsEnumerator ratiosEnumerator = EnumerateRatios(pMetricsEvaluator);
            const MetricsEnumerator throughputsEnumerator = EnumerateThroughputs(pMetricsEvaluator);
            size_t metricIndex = 0;
            size_t metricIndexPrev = metricIndex;
            for (size_t metricType = 0; metricType < NVPW_METRIC_TYPE__COUNT; ++metricType)
            {
                if (metricType)
                {
                    sstream << ", ";
                }

                if (metricType == NVPW_METRIC_TYPE_COUNTER)
                {
                    sstream << "\"rangesCounters\": {\n";
                }
                else if (metricType == NVPW_METRIC_TYPE_RATIO)
                {
                    sstream << "\"rangesRatios\": {\n";
                }
                else if (metricType == NVPW_METRIC_TYPE_THROUGHPUT)
                {
                    sstream << "\"rangesThroughputs\": {\n";
                }

                const auto& perTypeBaseMetrics = reportLayout.summary.baseMetricRequests.requests[metricType];
                const auto& perTypeSubmetrics = reportLayout.summary.submetricRequests.requests[metricType];
                for (size_t rangeIndex = 0; rangeIndex < reportData.ranges.size(); ++rangeIndex)
                {
                    if (rangeIndex)
                    {
                        sstream << ", ";
                    }

                    sstream << "\"" << reportData.ranges[rangeIndex].fullName << "\": {\n";
                    metricIndex = metricIndexPrev;
                    const std::vector<double>& metricValues = reportData.ranges[rangeIndex].summaryReportValues;
                    for (size_t baseMetricIndex = 0; baseMetricIndex < perTypeBaseMetrics.size(); ++baseMetricIndex)
                    {
                        const BaseMetricRequests::Request& baseMetric = perTypeBaseMetrics[baseMetricIndex];

                        if (baseMetricIndex)
                        {
                            sstream << ", ";
                        }

                        sstream << "\"" << ToCString(countersEnumerator, ratiosEnumerator, throughputsEnumerator, static_cast<NVPW_MetricType>(metricType), baseMetric.metricIndex) << "\": { ";
                        for (size_t submetricIndex = 0; submetricIndex < perTypeSubmetrics.size(); ++submetricIndex)
                        {
                            const SubmetricRequests::Request& submetric = perTypeSubmetrics[submetricIndex];

                            if (submetricIndex)
                            {
                                sstream << ", ";
                            }

                            std::string submetricName;
                            if (metricType == NVPW_METRIC_TYPE_COUNTER || metricType == NVPW_METRIC_TYPE_THROUGHPUT)
                            {
                                submetricName += ToCString(submetric.rollupOp);
                            }
                            submetricName += ToCString(submetric.submetric);
                            sstream << "\"" << submetricName.substr(1) << "\": " << FormatJsDouble(metricValues[metricIndex++]);
                        }
                        sstream << " }\n";
                    }
                    sstream << "}\n";
                }
                sstream << "}\n";
                metricIndexPrev = metricIndex;
            }
            const size_t totalNumSubmetrics = GetTotalNumSubmetrics(reportLayout.summary.baseMetricRequests, reportLayout.summary.submetricRequests);
            assert(metricIndex == totalNumSubmetrics);

            std::string jsonContents = sstream.str();
            return jsonContents;
        }

        inline void WriteHtmlReportFile(NVPW_MetricsEvaluator* pMetricsEvaluator, const ReportLayout& reportLayout, const ReportData& reportData)
        {
            const int filenameLength = snprintf(nullptr, 0, "%s%csummary.html", reportData.reportDirectoryName.c_str(), NV_PERF_PATH_SEPARATOR);
            std::vector<char> filename(filenameLength + 1);
            snprintf(&filename[0], filename.size(), "%s%csummary.html", reportData.reportDirectoryName.c_str(), NV_PERF_PATH_SEPARATOR);

            if (FILE* pHTMLFile = OpenFile(&filename[0], "wb"))
            {
                const std::string jsonContents = MakeJsonContents(pMetricsEvaluator, reportLayout, reportData);
                const std::string reportHtml = MakeReport(reportLayout.summary.definition, jsonContents);
                fputs(reportHtml.c_str(), pHTMLFile);
                fclose(pHTMLFile);
            }
            else
            {
                NV_PERF_LOG_ERR(20, "OpenFile failed for file: %s\n", &filename[0]);
            }
        }

        inline void WriteCsvReportFile(NVPW_MetricsEvaluator* pMetricsEvaluator, const ReportLayout& reportLayout, const ReportData& reportData)
        {
            const std::string filename = reportData.reportDirectoryName + NV_PERF_PATH_SEPARATOR + "nvperf_metrics_summary.csv";
            FILE* fp = OpenFile(filename.c_str(), "wt");
            if (!fp)
            {
                NV_PERF_LOG_ERR(20, "OpenFile failed for file: %s\n", filename.c_str());
                return;
            }

            // print header
            const MetricsEnumerator countersEnumerator = EnumerateCounters(pMetricsEvaluator);
            const MetricsEnumerator ratiosEnumerator = EnumerateRatios(pMetricsEvaluator);
            const MetricsEnumerator throughputsEnumerator = EnumerateThroughputs(pMetricsEvaluator);
            fprintf(fp, "\"Range Name\",");
            ForEachBaseMetric(reportLayout.summary.baseMetricRequests, reportLayout.summary.submetricRequests, [&](const NVPW_MetricEvalRequest* pMetricEvalRequests, size_t numMetricEvalRequests) {
                for (size_t ii = 0; ii < numMetricEvalRequests; ++ii)
                {
                    fprintf(fp, "\"%s\",", ToString(countersEnumerator, ratiosEnumerator, throughputsEnumerator, pMetricEvalRequests[ii]).c_str());
                }
                return true;
            });
            fprintf(fp, "\n");

            // print ranges
            for (const ReportData::RangeData& rangeData : reportData.ranges)
            {
                fprintf(fp, "\"%s\",", rangeData.fullName.c_str());
                for (double metricValue : rangeData.summaryReportValues)
                {
                    fprintf(fp, "%g,", metricValue);
                }
                fprintf(fp, "\n");
            }

            fclose(fp);
        }

    } // namespace Summary

} } // namespace nv::perf

namespace nv { namespace perf { namespace profiler {
    enum class ReportGeneratorInitStatus
    {
        NeverCalled,
        Reset,
        Failed,
        Succeeded
    };

    inline const char* ToCString(ReportGeneratorInitStatus initStatus)
    {
        switch (initStatus)
        {
            case ReportGeneratorInitStatus::NeverCalled:    return "was never called";
            case ReportGeneratorInitStatus::Reset:          return "was later Reset";
            case ReportGeneratorInitStatus::Failed:         return "previously failed";
            case ReportGeneratorInitStatus::Succeeded:      return "previously succeeded";
            default:                                        return "unknown (corruption)";
        }
    }

    class ReportGeneratorStateMachine
    {
    public: // types
        struct IReportProfiler
        {
            virtual bool IsInSession() const = 0;
            virtual bool IsInPass() const = 0;
            virtual bool EndSession() = 0;
            virtual bool EnqueueCounterCollection(const SetConfigParams& config) = 0;
            virtual bool BeginPass() = 0;
            virtual bool EndPass() = 0;
            virtual bool PushRange(const char* pRangeName) = 0;
            virtual bool PopRange() = 0;
            virtual bool DecodeCounters(DecodeResult& decodeResult) = 0;
            virtual bool AllPassesSubmitted() const = 0;
        };

        enum { MaxNumRangesDefault = 512 };

    protected:
        MetricsEvaluator m_metricsEvaluator;
        CounterConfiguration m_configuration;

        ReportLayout m_reportLayout;

        size_t m_deviceIndex;
        NVPW_Device_ClockStatus m_clockStatus;
        IReportProfiler& m_reportProfiler;

        // options
        std::string m_frameLevelRangeName;
        uint16_t m_numNestingLevels;
        bool m_openReportDirectoryAfterCollection;

        // state machine
        bool m_explicitSession;
        std::string m_reportDirectoryName;
        uint64_t m_collectionTime;
        bool m_setConfigDone;

    protected:
        template <class TBeginSession>
        bool BeginSessionImpl(TBeginSession&& beginSession)
        {
            if (!beginSession())
            {
                return false;
            }

            if (!m_setConfigDone)
            {
                if (!m_reportProfiler.EnqueueCounterCollection(SetConfigParams(m_configuration, m_numNestingLevels)))
                {
                    NV_PERF_LOG_ERR(10, "m_reportProfiler.EnqueueCounterCollection failed\n");
                    return false;
                }
                m_setConfigDone = true;
            }

            return true;
        }

        void EvalRangeMetricValues(
            const ReportData& reportData,
            const BaseMetricRequests& baseMetricRequests,
            const SubmetricRequests& submetricRequests,
            size_t rangeIndex,
            std::vector<double>& metricValues)
        {
            const size_t totalNumSubmetrics = GetTotalNumSubmetrics(baseMetricRequests, submetricRequests);
            metricValues.resize(totalNumSubmetrics);

            size_t metricIndex = 0;
            const bool evalSuccess = ForEachBaseMetric(
                baseMetricRequests,
                submetricRequests,
                [&](const NVPW_MetricEvalRequest* pMetricEvalRequests, size_t numMetricEvalRequests) {
                    const bool success = EvaluateToGpuValues(
                        m_metricsEvaluator,
                        reportData.pCounterDataImage,
                        reportData.counterDataImageSize,
                        rangeIndex,
                        numMetricEvalRequests,
                        pMetricEvalRequests,
                        metricValues.data() + metricIndex);
                    if (!success)
                    {
                        return false;
                    }
                    metricIndex += numMetricEvalRequests;
                    return true;
            });
            if (!evalSuccess)
            {
                NV_PERF_LOG_ERR(20, "Failed to evaluate metrics\n");
                return;
            }
            assert(metricIndex == totalNumSubmetrics);
        }

    public:
        ~ReportGeneratorStateMachine()
        {
            Reset();
        }

        ReportGeneratorStateMachine(IReportProfiler& reportProfiler)
            : m_metricsEvaluator()
            , m_configuration()
            , m_reportLayout()
            , m_deviceIndex(size_t(~0))
            , m_clockStatus(NVPW_DEVICE_CLOCK_STATUS_UNKNOWN)
            , m_reportProfiler(reportProfiler)
            , m_frameLevelRangeName()
            , m_numNestingLevels(1)
            , m_openReportDirectoryAfterCollection(false)
            , m_explicitSession(false)
            , m_reportDirectoryName()
            , m_collectionTime()
            , m_setConfigDone(false)
        {
            std::string envValue;
            if (GetEnvVariable("NV_PERF_OPEN_REPORT_DIR_AFTER_COLLECTION", envValue))
            {
                char* pEnd = nullptr;
                m_openReportDirectoryAfterCollection = !!strtol(envValue.c_str(), &pEnd, 0);
            }
        }

        void Reset()
        {
            m_setConfigDone = false;
            m_reportDirectoryName.clear();
            m_collectionTime = 0;
            m_explicitSession = false;

            m_numNestingLevels = 1;
            m_frameLevelRangeName.clear();

            m_reportLayout = {};

            m_deviceIndex = size_t(~0);
            m_clockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN;

            m_configuration = {};
            m_metricsEvaluator = {};
        }

        template <class TCreateMetricsEvaluator, class TCreateRawMetricsConfig>
        bool InitializeReportMetrics(
            size_t deviceIndex,
            const DeviceIdentifiers& deviceIdentifiers,
            TCreateMetricsEvaluator&& createMetricsEvaluator,
            TCreateRawMetricsConfig&& createRawMetricsConfig,
            const std::vector<std::string>& additionalMetrics)
        {
            m_deviceIndex = deviceIndex;

            // initialize metrics evaluator
            std::vector<uint8_t> metricsEvaluatorScratchBuffer;
            NVPW_MetricsEvaluator* pMetricsEvaluator = createMetricsEvaluator(metricsEvaluatorScratchBuffer);
            if (!pMetricsEvaluator)
            {
                return false;
            }
            m_metricsEvaluator = MetricsEvaluator(pMetricsEvaluator, std::move(metricsEvaluatorScratchBuffer)); // transfer ownership to m_metricsEvaluator

            // initialize report definition and report data - per-range report
            m_reportLayout.perRange.definition = nv::perf::PerRangeReport::GetReportDefinition(deviceIdentifiers.pChipName);
            if (!m_reportLayout.perRange.definition.pReportHtml)
            {
                NV_PERF_LOG_ERR(10, "HTML Reports not supported for chip=%s, Device=%s\n", deviceIdentifiers.pChipName, deviceIdentifiers.pDeviceName);
                return false;
            }
            nv::perf::PerRangeReport::InitReportDataMetrics(m_metricsEvaluator, additionalMetrics, m_reportLayout);

            // initialize report definition and report data - ranges summary report
            m_reportLayout.summary.definition = nv::perf::SummaryReport::GetReportDefinition(deviceIdentifiers.pChipName);
            if (!m_reportLayout.summary.definition.pReportHtml)
            {
                NV_PERF_LOG_ERR(10, "HTML Reports not supported for chip=%s, Device=%s\n", deviceIdentifiers.pChipName, deviceIdentifiers.pDeviceName);
                return false;
            }
            nv::perf::SummaryReport::InitReportDataMetrics(m_metricsEvaluator, m_reportLayout);

            m_reportLayout.gpuName = deviceIdentifiers.pDeviceName;
            m_reportLayout.chipName = deviceIdentifiers.pChipName;

            // initialize raw metric config
            NVPA_RawMetricsConfig* pRawMetricsConfig = createRawMetricsConfig();
            if (!pRawMetricsConfig)
            {
                NV_PERF_LOG_ERR(10, "RawMetricsConfig creation failed\n");
                return false;
            }

            // add metrics and create configuration
            nv::perf::MetricsConfigBuilder configBuilder;
            if (!configBuilder.Initialize(m_metricsEvaluator, pRawMetricsConfig, deviceIdentifiers.pChipName))
            {
                NV_PERF_LOG_ERR(10, "configBuilder.Initialize() failed\n");
                return false;
            }
            auto addMetrics = [&](const NVPW_MetricEvalRequest* pMetricEvalRequests, size_t numMetricEvalRequests) {
                if (!configBuilder.AddMetrics(pMetricEvalRequests, numMetricEvalRequests))
                {
                    return false;
                }
                return true;
            };
            if (!ForEachBaseMetric(m_reportLayout.perRange.baseMetricRequests, m_reportLayout.perRange.submetricRequests, addMetrics))
            {
                NV_PERF_LOG_ERR(10, "AddMetrics failed for per-range report\n");
                return false;
            }
            if (!ForEachBaseMetric(m_reportLayout.summary.baseMetricRequests, m_reportLayout.summary.submetricRequests, addMetrics))
            {
                NV_PERF_LOG_ERR(10, "AddMetrics failed for summary report\n");
                return false;
            }
            if (!nv::perf::CreateConfiguration(configBuilder, m_configuration))
            {
                NV_PERF_LOG_ERR(10, "CreateConfiguration failed\n");
                return false;
            }

            return true;
        }

        template <class TBeginSession>
        bool BeginSession(TBeginSession&& beginSession)
        {
            bool status = BeginSessionImpl(std::forward<TBeginSession>(beginSession));
            if (status)
            {
                return false;
            }
            m_explicitSession = true;
            return true;
        }

        bool EndSession()
        {
            if (!m_reportProfiler.EndSession())
            {
                NV_PERF_LOG_ERR(10, "m_reportProfiler.EndSession failed\n");
                return false;
            }

            return true;
        }

        template <class TBeginSession>
        bool OnFrameStart(TBeginSession&& beginSessionFn)
        {
            if (IsCollectingReport())
            {
                if (!m_reportProfiler.IsInSession())
                {
                    if (!BeginSessionImpl(beginSessionFn))
                    {
                        NV_PERF_LOG_ERR(10, "BeginSession failed\n");
                        m_reportDirectoryName.clear();
                        return false;
                    }
                }

                if (!m_reportProfiler.AllPassesSubmitted())
                {
                    if (!m_reportProfiler.BeginPass())
                    {
                        NV_PERF_LOG_ERR(10, "BeginPass failed\n");
                        ResetCollection();
                        return false;
                    }

                    if (!m_frameLevelRangeName.empty())
                    {
                        if (!m_reportProfiler.PushRange(m_frameLevelRangeName.c_str()))
                        {
                            NV_PERF_LOG_ERR(10, "m_reportProfiler.PushRange failed\n");
                            ResetCollection();
                            return false;
                        }
                    }
                }
            }

            return true;
        }

        bool OnFrameEnd(const ReportOutputOptions& outputOptions)
        {
            if (IsCollectingReport())
            {
                if (!m_reportProfiler.AllPassesSubmitted() && m_reportProfiler.IsInPass())
                {
                    if (!m_frameLevelRangeName.empty())
                    {
                        if (!m_reportProfiler.PopRange())
                        {
                            NV_PERF_LOG_ERR(10, "m_reportProfiler.PopRange failed\n");
                            ResetCollection();
                            return false;
                        }
                    }

                    if (!m_reportProfiler.EndPass())
                    {
                        NV_PERF_LOG_ERR(10, "m_reportProfiler.EndPass failed\n");
                        ResetCollection();
                        return false;
                    }
                }

                DecodeResult decodeResult = {};
                if (!m_reportProfiler.DecodeCounters(decodeResult))
                {
                    NV_PERF_LOG_ERR(10, "m_reportProfiler.DecodeCounters failed\n");
                    ResetCollection();
                    return false;
                }

                if (decodeResult.allStatisticalSamplesCollected)
                {
                    [&]() {
                        bool setDeviceSuccess = MetricsEvaluatorSetDeviceAttributes(m_metricsEvaluator, decodeResult.counterDataImage.data(), decodeResult.counterDataImage.size());
                        if (!setDeviceSuccess)
                        {
                            NV_PERF_LOG_ERR(50, "MetricsEvaluatorSetDeviceAttributes failed, skipping writing report files\n");
                            return;
                        }

                        ReportData reportData = {};
                        reportData.secondsSinceEpoch = m_collectionTime;
                        reportData.clockStatus = m_clockStatus;
                        reportData.pCounterDataImage = decodeResult.counterDataImage.data();
                        reportData.counterDataImageSize = decodeResult.counterDataImage.size();
                        reportData.reportDirectoryName = m_reportDirectoryName;

                        const size_t numRanges = nv::perf::CounterDataGetNumRanges(reportData.pCounterDataImage);
                        reportData.ranges.resize(numRanges);
                        for (size_t rangeIndex = 0; rangeIndex < numRanges; rangeIndex++)
                        {
                            const char* pLeafName = nullptr;
                            reportData.ranges[rangeIndex].fullName = CounterDataGetRangeName(reportData.pCounterDataImage, rangeIndex, '/', &pLeafName);
                            reportData.ranges[rangeIndex].leafName = pLeafName;

                            EvalRangeMetricValues(
                                reportData,
                                m_reportLayout.summary.baseMetricRequests,
                                m_reportLayout.summary.submetricRequests,
                                rangeIndex,
                                reportData.ranges[rangeIndex].summaryReportValues);
                            EvalRangeMetricValues(
                                reportData,
                                m_reportLayout.perRange.baseMetricRequests,
                                m_reportLayout.perRange.submetricRequests,
                                rangeIndex,
                                reportData.ranges[rangeIndex].perRangeReportValues);
                        }

                        if (outputOptions.enableHtmlReport)
                        {
                            SummaryReport::WriteHtmlReportFile(m_metricsEvaluator, m_reportLayout, reportData);
                            PerRangeReport::WriteHtmlReportFiles(m_metricsEvaluator, m_reportLayout, reportData);
                        }

                        if (outputOptions.enableCsvReport)
                        {
                            SummaryReport::WriteCsvReportFile(m_metricsEvaluator, m_reportLayout, reportData);
                            PerRangeReport::WriteCsvReportFile(m_metricsEvaluator, m_reportLayout, reportData);
                        }

                        for (const ReportWriterFn& reportWriter : outputOptions.reportWriters)
                        {
                            reportWriter(m_metricsEvaluator, m_reportLayout, reportData);
                        }

                        if (m_openReportDirectoryAfterCollection)
                        {
#if defined(_WIN32)
                            intptr_t shellExecResult = (intptr_t)ShellExecuteA(NULL, "explore", m_reportDirectoryName.c_str(), NULL, NULL, SW_SHOWNORMAL);
                            if (shellExecResult <= 32)
                            {
                                NV_PERF_LOG_WRN(50, "Failed to open directory %s, ShellExecute() returned %p\n", m_reportDirectoryName.c_str(), shellExecResult);
                            }
#elif defined(__linux__)
                            pid_t pid = fork();
                            if (pid == -1)
                            {
                                NV_PERF_LOG_WRN(50, "fork() failed, errno = %d \n", errno);
                            }
                            else if (pid == 0)
                            {
                                // Only child process enter this block
                                FILE* pStdOut = freopen("/dev/null", "w", stdout);
                                NV_PERF_UNUSED_VARIABLE(pStdOut);
                                FILE* pStdErr = freopen("/dev/null", "w", stderr);
                                NV_PERF_UNUSED_VARIABLE(pStdErr);
                                if (execlp("xdg-open", "xdg-open", m_reportDirectoryName.c_str(), nullptr) == -1)
                                {
                                    NV_PERF_LOG_WRN(50, "execlp() failed, errno = %d \n", errno);
                                    exit(0);
                                }
                            }
#endif
                        }
                    }();

                    m_reportDirectoryName.clear();
                    m_collectionTime = 0;
                    if (!m_explicitSession)
                    {
                        if (!m_reportProfiler.EndSession())
                        {
                            NV_PERF_LOG_ERR(50, "m_reportProfiler.EndSession failed\n");
                            return false;
                        }
                        m_setConfigDone = false;
                    }
                }
            }

            return true;
        }

        // Request to start collection on the next FrameStart.
        bool StartCollectionOnNextFrame(const char* pDirectoryName, AppendDateTime appendDateTime)
        {
            if (!m_reportDirectoryName.empty())
            {
                return true; // there is already one active collection session going on
            }
            std::string reportDirectoryName(pDirectoryName);
            if (appendDateTime == AppendDateTime::yes)
            {
                auto now = std::chrono::system_clock::now();
                const time_t secondsSinceEpoch = std::chrono::system_clock::to_time_t(now);
                m_collectionTime = static_cast<uint64_t>(secondsSinceEpoch);
                const std::string formattedTime = FormatTime(secondsSinceEpoch);
                if (!formattedTime.empty())
                {
                    reportDirectoryName += std::string(1, NV_PERF_PATH_SEPARATOR) + formattedTime;
                }
            }
            auto isPathSeparator = [](char c) {
                return (c == NV_PERF_PATH_SEPARATOR);
            };
            if (!isPathSeparator(reportDirectoryName.back()))
            {
                reportDirectoryName += NV_PERF_PATH_SEPARATOR;
            }

            // try to recursively create the directory
            for (size_t di = 0; di < reportDirectoryName.length(); ++di)
            {
                if (isPathSeparator(reportDirectoryName[di]))
                {
                    std::string parentDir(reportDirectoryName, 0, di);
#ifdef WIN32
                    BOOL dirCreated = CreateDirectoryA(parentDir.c_str(), NULL);
#else
                    bool dirCreated = !mkdir(parentDir.c_str(), 0777);
#endif
                    if (!dirCreated) { /* it probably already exists */ }
                }
            }

            // try to create a file in the directory
            {
                const std::string filename = reportDirectoryName + NV_PERF_PATH_SEPARATOR + "readme.html";
                FILE* fp = OpenFile(filename.c_str(), "wt");
                if (!fp)
                {
                    NV_PERF_LOG_ERR(50, "Failed to create files in directory %s, data collection might be skipped\n", reportDirectoryName.c_str());
                    return false; // note IsCollectingReport() will return false
                }
                fprintf(fp, "%s", GetReadMeHtml().c_str());
                fclose(fp);
            }
            m_clockStatus = GetDeviceClockState(m_deviceIndex);
            m_reportDirectoryName = reportDirectoryName;
            return true;
        }

        void ResetCollection()
        {
            m_reportDirectoryName.clear();
            m_collectionTime = 0;
            m_clockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN;
            if (m_reportProfiler.IsInSession())
            {
                m_reportProfiler.EndSession();
            }
            m_setConfigDone = false;
            m_explicitSession = false;
        }

        // Returns true if a report is still queued for collection.
        bool IsCollectingReport() const
        {
            return !m_reportDirectoryName.empty();
        }

        const std::string& GetReportDirectoryName() const
        {
            return m_reportDirectoryName;
        }

        /// Enables a frame-level parent range.
        /// When enabled (non-NULL, non-empty pRangeName), every frame will have a parent range.
        /// This is also convenient for programs that have no CommandList-level ranges.
        /// Pass in NULL or an empty string to disable this behavior.
        /// The pRangeName string is copied by value, and may be modified or freed after this function returns.
        void SetFrameLevelRangeName(const char* pRangeName)
        {
            if (pRangeName)
            {
                m_frameLevelRangeName = pRangeName;
            }
            else
            {
                m_frameLevelRangeName.clear();
            }
        }

        const std::string& GetFrameLevelRangeName() const
        {
            return m_frameLevelRangeName;
        }

        // For richly instrumented engines, set this to the maximum number of nesting levels.
        // Example: to measure the following code sequence, set numNestingLevels = 3
        //      PushRange("Frame"), PushRange("Scene"), PushRange("Character"), PopRange(), PopRange(), PopRange()
        void SetNumNestingLevels(uint16_t numNestingLevels)
        {
            m_numNestingLevels = numNestingLevels ? numNestingLevels : 1;
        }

        uint16_t GetNumNestingLevels() const
        {
            return m_numNestingLevels;
        }

        void SetOpenReportDirectoryAfterCollection(bool openReportDirectoryAfterCollection)
        {
            m_openReportDirectoryAfterCollection = openReportDirectoryAfterCollection;
        }
    };

} } }

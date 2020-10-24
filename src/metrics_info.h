#pragma once

#include "def.h"
#include <memory>
#include <string>
#include "../3rdparty/CImg.h"

enum MetricType
{
    METRIC_SM_SOL,
    METRIC_FB_USAGE,
    METRIC_MEM_SOL,
    METRIC_NVENC_SOL,
    METRIC_NVDEC_SOL,
    METRIC_SM_CLK,
    METRIC_MEM_CLK,
    METRIC_PCIE_TX,
    METRIC_PCIE_RX,
    METRIC_NVLINK_TX,
    METRIC_NVLINK_RX,

    METRIC_CPU_SOL,
    METRIC_SYS_MEM_SOL,
    METRIC_DISK_READ_SOL,
    METRIC_DISK_WRITE_SOL,

    METRIC_FPS_0,
    METRIC_FPS_1,
    METRIC_FPS_2,
    METRIC_FPS_3,
    METRIC_FPS_4,
    METRIC_FPS_5,

    METRIC_COUNT,
};

const uint8_t colors[][3] =
{
    { 255,255,255 },
    { 195,38,114 },
    { 69,203,209},
    { 138,226,36 },
    { 174,122,169 },
    { 200,122,10 },
    { 122,200,10 },
    { 10,122,200 },
    { 122,122,122 },
    { 200,122,10 },
    { 10,122,200 },
};

extern std::string kMetricNames[METRIC_COUNT];

struct MetricsInfo
{
    static const int HISTORY_COUNT = WINDOW_W / 2;
    float metrics[METRIC_COUNT][HISTORY_COUNT] = {};
    float metrics_sum[METRIC_COUNT] = {};
    float metrics_avg[METRIC_COUNT] = {};
    int valid_element_count[METRIC_COUNT] = {};

    void addMetric(MetricType type, float value);
    void resetMetric(MetricType type);

    void draw(std::shared_ptr<cimg_library::CImgDisplay> window, cimg_library::CImg<unsigned char>& img, int beginMetricId, int endMetricId, bool absoluteValue = false);
    void drawImgui(const char* panelName, int beginMetricId, int endMetricId, bool absoluteValue = false);
};

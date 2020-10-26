#include "../3rdparty/CImg.h"
#include "metrics_info.h"
#include "../3rdparty/imgui/imgui.h"

using namespace cimg_library;
using namespace std;

MetaType kMetricMetas[METRIC_COUNT] =
{
    {"SM", "%"},
    {"RAM", "%"},
    {"MEM", "%"},
    {"TEMP", "C"},
    {"POWER", "W"},
    {"ENC", "%"},
    {"DEC", "%"},
    {"SM CLK", "%"},
    {"MEM CLK", "%"},
    {"PCIE TX", "%"},
    {"PCIE RX", "%"},
    {"NVLK TX", "%"},
    {"NVLK RX", "%"},

    {"CPU", "%"},
    {"RAM", "%"},
    {"DISK R", "%"},
    {"DISK W", "%"},

    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
};

const size_t COLOR_COUNT = _countof(colors);

void MetricsInfo::addMetric(MetricType type, float value)
{
    metrics_sum[type] -= metrics[type][0];
    metrics_sum[type] += value;

    if (valid_element_count[type] < MetricsInfo::HISTORY_COUNT)
        valid_element_count[type]++;
    metrics_avg[type] = metrics_sum[type] / valid_element_count[type];
    for (int i = 0; i < HISTORY_COUNT - 1; i++)
        metrics[type][i] = metrics[type][i + 1];
    metrics[type][HISTORY_COUNT - 1] = value;
}

extern int global_mouse_x;
extern int global_mouse_y;

void MetricsInfo::draw(shared_ptr<CImgDisplay> window, CImg<unsigned char>& img, int beginMetricId, int endMetricId)
{
    const int plotType = 1;
    const int vertexType = 1;
    const float alpha = 0.5f;

    unsigned int hatch = 0xF0F0F0F0;

    // metrics charts
    for (int k = beginMetricId; k <= endMetricId; k++)
    {
        CImg<float> plot(metrics[k], MetricsInfo::HISTORY_COUNT, 1);
        img.draw_graph(plot, colors[k % COLOR_COUNT], alpha, plotType, vertexType, 102, 0);
    }

    // avg summary
    for (int k = beginMetricId; k <= endMetricId; k++)
    {
        img.draw_text(FONT_HEIGHT, FONT_HEIGHT * (k - beginMetricId + 1),
            "%s: %.1f%s\n",
            colors[k % COLOR_COUNT], 0, 1, FONT_HEIGHT,
            kMetricMetas[k].name.c_str(),
            metrics_avg[k],
            kMetricMetas[k].suffix.c_str());
    }

    // point tooltip
    if (global_mouse_x >= 0 && global_mouse_y >= 0)
    {
        auto value_idx = global_mouse_x / 2;
        for (int k = beginMetricId; k <= endMetricId; k++)
        {
            img.draw_text(window->window_width() - 100, FONT_HEIGHT * (k - beginMetricId + 1),
                "|%.1f%s\n",
                colors[k % COLOR_COUNT], 0, 1, FONT_HEIGHT,
                metrics[k][value_idx],
                kMetricMetas[k].suffix.c_str()
            );
        }
        img.draw_line(global_mouse_x, 0, global_mouse_x, window->height() - 1, colors[0], 0.5f, hatch = cimg::rol(hatch));
    }
}

void MetricsInfo::resetMetric(MetricType type)
{
    valid_element_count[type] = 0;
    metrics_sum[type] = 0;
    metrics_avg[type] = 0;
    for (int k = 0; k < MetricsInfo::HISTORY_COUNT; k++)
    {
        metrics[type][k] = 0;
    }
}

void MetricsInfo::drawImgui(const char* panelName, int beginMetricId, int endMetricId)
{
    for (int k = beginMetricId; k <= endMetricId; k++)
    {
        char label[32];
        sprintf(label, "%s - %s", panelName, kMetricMetas[k].name.c_str());
        char overlay[32];
        sprintf(overlay, "avg %.1f%s", metrics_avg[k], kMetricMetas[k].suffix.c_str());
        ImGui::PlotLines(label, metrics[k], MetricsInfo::HISTORY_COUNT, 0, overlay, 0.0f, 30, ImVec2(0, 60));
        //img.draw_text(window->window_width() - 100, FONT_HEIGHT * (k - beginMetricId + 1),
        //    absoluteValue ? "|%.1f\n" : "|%.1f%%\n",
        //    colors[k % COLOR_COUNT], 0, 1, FONT_HEIGHT,
        //    metrics[k][value_idx]);
    }
    //img.draw_line(global_mouse_x, 0, global_mouse_x, window->height() - 1, colors[0], 0.5f, hatch = cimg::rol(hatch));
}

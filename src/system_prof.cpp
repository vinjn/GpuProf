#include "system_prof.h"
#include "../3rdparty/PDH/CPdh.h"
#include "../3rdparty/CImg.h"
#include "metrics_info.h"
using namespace cimg_library;
using namespace std;

// TODO
extern vector<shared_ptr<CImgDisplay>> windows;
extern bool isCanvasVisible;

namespace
{
    CPDH pdh;

    MetricsInfo metrics;
    shared_ptr<CImgDisplay> window;

    // TODO: refactor

    /// Add Counter ///
    int nIdx_CpuUsage = -1;
    int nIdx_MemUsage = -1;
    int nIdx_DiskRead = -1;
    int nIdx_DiskWrite = -1;
    int nIdx_NetRead = -1;
    int nIdx_NetWrite = -1;
    int nIdx_NetBandwidth = -1;
    double dCpu = 0;
    double dMem = 0;
    double diskRead = 0;
    double diskWrite = 0;

    double netRead = 0;
    double netWrite = 0;
    double netBandwidth = 0;

};

int system_setup()
{

    pdh.AddCounter(df_PDH_CPUUSAGE_TOTAL, nIdx_CpuUsage);
    pdh.AddCounter(df_PDH_MEMINUSE_PERCENT, nIdx_MemUsage);
    pdh.AddCounter(df_PDH_DISK_READ_TOTAL, nIdx_DiskRead);
    pdh.AddCounter(df_PDH_DISK_WRITE_TOTAL, nIdx_DiskWrite);

    pdh.AddCounter(df_PDH_ETHERNETRECV_BYTES, nIdx_NetRead);
    pdh.AddCounter(df_PDH_ETHERNETSEND_BYTES, nIdx_NetWrite);
    pdh.AddCounter(df_PDH_ETHERNET_BANDWIDTH, nIdx_NetBandwidth);


    if (isCanvasVisible)
    {
        window = make_shared<CImgDisplay>(WINDOW_W, WINDOW_H, "System", 3);
        window->move(400, 100);
        windows.push_back(window);
    }

    return 0;
}

int system_update()
{
    if (pdh.CollectQueryData())
        return 1;

    /// Update Counters ///
    if (!pdh.GetCounterValue(nIdx_CpuUsage, &dCpu)) dCpu = 0;
    if (!pdh.GetCounterValue(nIdx_MemUsage, &dMem)) dMem = 0;
    if (!pdh.GetCounterValue(nIdx_DiskRead, &diskRead)) diskRead = 0;
    if (!pdh.GetCounterValue(nIdx_DiskWrite, &diskWrite)) diskWrite = 0;

    if (!pdh.GetCounterValue(nIdx_NetRead, &netRead)) netRead = 0;
    if (!pdh.GetCounterValue(nIdx_NetWrite, &netWrite)) netWrite = 0;
    if (!pdh.GetCounterValue(nIdx_NetBandwidth, &netBandwidth)) netBandwidth = 0.1f;

#if 0
    double dMin = 0, dMax = 0, dMean = 0;
    if (pdh.GetStatistics(&dMin, &dMax, &dMean, nIdx_CpuUsage))
        wprintf(L" (Min %.1f / Max %.1f / Mean %.1f)", dMin, dMax, dMean);
#endif
    metrics.addMetric(METRIC_CPU_SOL, dCpu);
    metrics.addMetric(METRIC_SYS_MEM_SOL, dMem);
    metrics.addMetric(METRIC_DISK_READ_SOL, diskRead);
    metrics.addMetric(METRIC_DISK_WRITE_SOL, diskWrite);

    metrics.addMetric(METRIC_NET_READ_SOL, netRead * 800 / netBandwidth);
    metrics.addMetric(METRIC_NET_WRITE_SOL, netWrite * 800 / netBandwidth);

    return 0;
}

int system_draw()
{
    CImg<unsigned char> img(window->width(), window->height(), 1, 3, 50);
    img.draw_grid(-50 * 100.0f / window->width(), -50 * 100.0f / 256, 0, 0, false, true, colors[0], 0.2f, 0xCCCCCCCC, 0xCCCCCCCC);

    metrics.draw(window, img, METRIC_CPU_SOL, METRIC_NET_WRITE_SOL);

    img.display(*window);
    return 0;
}

int system_draw_imgui()
{
    metrics.drawImgui("System", METRIC_CPU_SOL, METRIC_NET_WRITE_SOL);

    return 0;
}

int system_cleanup()
{
    return 0;
}


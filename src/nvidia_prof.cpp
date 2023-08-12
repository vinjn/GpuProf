#include "nvidia_prof.h"
#include <stdio.h>
#include <assert.h>
#include <string>
#include <inttypes.h>
#include "util_win32.h"
#include <memory>
#include <vector>

#include "../3rdparty/CImg.h"
#include "metrics_info.h"
using namespace cimg_library;
using namespace std;

#include "../3rdparty/CUDA_SDK/nvml.h"

//#define NV_PERF_ENABLE_INSTRUMENTATION

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#ifdef _WIN32
// Suppress redifinition warnings
//#undef APIENTRY
// undef min and max from windows.h
#define NOMINMAX
#endif
#include "windows-desktop-x64/nvperf_host_impl.h"
#include "../3rdparty/NvPerfUtility/include/NvPerfReportGenerator.h"
#include "../3rdparty/NvPerfUtility/include/NvPerfPeriodicSamplerGpu.h"
#include "../3rdparty/NvPerfUtility/include/NvPerfOpenGL.h"

//nv::perf::profiler::ReportGeneratorOpenGL m_nvperf;
double m_nvperfWarmupTime = 0.5; // Wait 0.5s to allow the clock to stabalize before begining to profile
NVPW_Device_ClockStatus m_clockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN; // Used to restore clock state when exiting

using namespace nv::perf;
using namespace nv::perf::sampler;

size_t nsightDeviceIndex = -1;

size_t GetCompatibleGpuDeviceIndex()
{
    NVPW_GetDeviceCount_Params getDeviceCountParams = { NVPW_GetDeviceCount_Params_STRUCT_SIZE };
    NVPA_Status nvpaStatus = NVPW_GetDeviceCount(&getDeviceCountParams);
    if (nvpaStatus)
    {
        NV_PERF_LOG_ERR(50, "Failed NVPW_GetDeviceCount: %u\n", nvpaStatus);
        return size_t(~0);
    }

    for (size_t deviceIndex = 0; deviceIndex < getDeviceCountParams.numDevices; ++deviceIndex)
    {
        if (GpuPeriodicSamplerIsGpuSupported(deviceIndex))
        {
            return deviceIndex;
        }
    }
    return size_t(~0);
}

#endif

// display information about the calling function and related error
void ShowErrorDetails(const nvmlReturn_t nvRetVal, const char* pFunctionName);

#define CHECK_NVML(nvRetValue, func) \
            if (NVML_SUCCESS != nvRetValue) \
            { \
                if (nvRetValue != NVML_ERROR_NO_PERMISSION && nvRetValue != NVML_ERROR_NOT_SUPPORTED) \
                { \
                    ShowErrorDetails(nvRetValue, #func); \
                } \
            }

#define ENTRY(func) decltype(func)  *_##func = NULL;
#include "../3rdparty/CUDA_SDK/nvml.def"
#undef ENTRY(func)

bool LoadNVML()
{
    // Load the NVML DLL using the default NVML DLL install path
    // NOTE: This DLL is included in the NVIDIA driver installation by default
    static HINSTANCE hDLLhandle = NULL;
    if (hDLLhandle) return true;

    const char* nvmlFilenames[] =
    {
        "C:\\Program Files\\NVIDIA Corporation\\NVSMI\\NVML.DLL",
        "C:\\Windows\\System32\\nvml.dll"
    };
    for (auto file : nvmlFilenames)
    {
        hDLLhandle = LoadLibraryA(file);
        if (hDLLhandle) break;
    }

    // if the DLL can not be found, exit
    if (NULL == hDLLhandle)
    {
        printf("NVML DLL is not installed or not found at the default path.\r\n");
        return false;
    }

#define ENTRY(func) _##func = (decltype(func)*)GetProcAddress(hDLLhandle, #func);
#include "../3rdparty/CUDA_SDK/nvml.def"
#undef ENTRY(func)

    return true;
}

int getUInt(nvmlDevice_t device, int fieldId, uint32_t* value)
{
    nvmlFieldValue_t fieldValue = {};
    fieldValue.fieldId = fieldId;
    nvmlReturn_t nvRetValue = NVML_ERROR_UNKNOWN;
    nvRetValue = _nvmlDeviceGetFieldValues(device, 1, &fieldValue);
    *value = 0;
    CHECK_NVML(nvRetValue, nvmlDeviceGetFieldValues);
    *value = fieldValue.value.uiVal;

    return 0;
}

// display information about the calling function and related error
void ShowErrorDetails(const nvmlReturn_t nvRetVal, const char* pFunctionName)
{
    auto pErrorDescription = _nvmlErrorString(nvRetVal);
    fprintf(stderr, "[%s] - %s\r\n", pFunctionName, pErrorDescription);
}

struct ProcInfo
{
    unsigned int pid;
    std::string exeName;
    PROCESSENTRY32 cpuStats;
    nvmlAccountingStats_t gpuStats;
};

struct NvidiaInfo
{
    shared_ptr<CImgDisplay> window;

    uint32_t deviceId = 0;
    nvmlDevice_t handle = NULL;
    nvmlPciInfo_t pciInfo;
    uint32_t numCores = 0;
    uint32_t busWidth = 0;
    uint32_t pcieLinkWidth = 0;
    uint32_t pcieLinkGeneration = 0;
    uint32_t pcieCurrentSpeed = 0;
    nvmlDriverModel_t driverModel, pendingDriverModel;
    nvmlBrandType_t brandType = NVML_BRAND_UNKNOWN;
    nvmlDeviceArchitecture_t deviceArch = NVML_DEVICE_ARCH_UNKNOWN;
    char cDevicename[NVML_DEVICE_NAME_BUFFER_SIZE] = { '\0' };
    uint32_t numLinks;
    nvmlEnableState_t nvlinkActives[NVML_NVLINK_MAX_LINKS];
    uint32_t nvlinkMaxSpeeds[NVML_NVLINK_MAX_LINKS];
    nvmlPciInfo_t nvlinkPciInfos[NVML_NVLINK_MAX_LINKS];

    std::vector<ProcInfo> ProcInfos;

    // Flags to denote unsupported queries
    bool bGPUUtilSupported = true;
    bool bEncoderUtilSupported = true;
    bool bDecoderUtilSupported = true;

    nvmlEnableState_t bMonitorConnected = NVML_FEATURE_DISABLED;

    MetricsInfo metrics;

    int setup();

    int update();

    int updatePerProcessInfo();

    void draw(bool show_legends);

    void drawImgui()
    {
        metrics.drawImgui(cDevicename, METRIC_SM_SOL, METRIC_FB_USAGE);
    }
};

int NvidiaInfo::setup()
{
    auto nvRetValue = _nvmlDeviceGetHandleByIndex_v2(deviceId, &handle);
    CHECK_NVML(nvRetValue, nvmlDeviceGetHandleByIndex);

    nvRetValue = _nvmlDeviceSetAccountingMode(handle, NVML_FEATURE_ENABLED);

    printf("%d", deviceId);
    nvRetValue = _nvmlDeviceGetPciInfo_v3(handle, &pciInfo);
    CHECK_NVML(nvRetValue, nvmlDeviceGetPciInfo);
    
    nvRetValue = _nvmlDeviceGetDisplayMode(handle, &bMonitorConnected);
    //CHECK_NVML(nvRetValue, nvmlDeviceGetDisplayMode);

    // nvlink
    getUInt(handle, NVML_FI_DEV_NVLINK_LINK_COUNT, &numLinks);
    assert(numLinks <= NVML_NVLINK_MAX_LINKS);
    for (int j = 0; j < numLinks; j++)
    {
        // TODO:
        //bNVLinkSupported = true;
        _nvmlDeviceGetNvLinkState(handle, j, &nvlinkActives[j]);
        _nvmlDeviceGetNvLinkRemotePciInfo_v2(handle, j, &nvlinkPciInfos[j]);
        getUInt(handle, NVML_FI_DEV_NVLINK_SPEED_MBPS_L0 + j, &nvlinkMaxSpeeds[j]);

        for (int counter = 0; counter < 2; counter++)
        {
            unsigned int reset = 1;
            nvmlNvLinkUtilizationControl_t control = { NVML_NVLINK_COUNTER_UNIT_BYTES, NVML_NVLINK_COUNTER_PKTFILTER_ALL };
            nvRetValue = _nvmlDeviceSetNvLinkUtilizationControl(handle, j, counter, &control, reset);
            //CHECK_NVML(nvRetValue, nvmlDeviceSetNvLinkUtilizationControl);
        }
    }

    // Get driver mode, WDDM or TCC?
    nvRetValue = _nvmlDeviceGetDriverModel(handle, &driverModel, &pendingDriverModel);
    CHECK_NVML(nvRetValue, nvmlDeviceGetDriverModel);
    static char* driverModelsString[] = { "WDDM", "TCC", "N/A" };
    printf("\t%s", driverModelsString[driverModel]);

    if (_nvmlDeviceGetNumGpuCores)
    {
        _nvmlDeviceGetNumGpuCores(handle, &numCores);
        printf("\t%u", numCores);
    }
    else printf("\tN/A");

    if (_nvmlDeviceGetMemoryBusWidth)
    {
        _nvmlDeviceGetMemoryBusWidth(handle, &busWidth);
        printf("\t%u", busWidth);
    }
    else printf("\tN/A");

    if (_nvmlDeviceGetCurrPcieLinkWidth && _nvmlDeviceGetCurrPcieLinkGeneration)
    {
        _nvmlDeviceGetCurrPcieLinkWidth(handle, &pcieLinkWidth);
        _nvmlDeviceGetCurrPcieLinkGeneration(handle, &pcieLinkGeneration);
        if (pcieLinkGeneration != 0)
            printf("\t%u.0 x%u", pcieLinkGeneration, pcieLinkWidth);
        else
            printf("\tN/A");
    }
    else printf("\tN/A");

    if (_nvmlDeviceGetPcieSpeed)
    {
        _nvmlDeviceGetPcieSpeed(handle, &pcieCurrentSpeed);
        printf("\t%.0f", pcieCurrentSpeed / 1e3);
    }
    else printf("\tN/A");

    // Get the device name
    nvRetValue = _nvmlDeviceGetName(handle, cDevicename, NVML_DEVICE_NAME_BUFFER_SIZE);
    CHECK_NVML(nvRetValue, nvmlDeviceGetName);

    nvRetValue = _nvmlDeviceGetBrand(handle, &brandType);
    //CHECK_NVML(nvRetValue, nvmlDeviceGetBrand);

    char* brandName = "";
#define ENTRY(type, desc) case type: brandName = desc; break;
    switch (brandType)
    {
        ENTRY(NVML_BRAND_UNKNOWN, "UNKNOWN");
        ENTRY(NVML_BRAND_QUADRO, "Quadro");
        ENTRY(NVML_BRAND_TESLA, "Tesla");
        ENTRY(NVML_BRAND_NVS, "NVS");
        ENTRY(NVML_BRAND_GRID, "GRID");
        ENTRY(NVML_BRAND_GEFORCE, "Geforce");
        ENTRY(NVML_BRAND_TITAN, "Titan");
        ENTRY(NVML_BRAND_NVIDIA_VAPPS, "vApps");
        ENTRY(NVML_BRAND_NVIDIA_VPC, "vPC");
        ENTRY(NVML_BRAND_NVIDIA_VCS, "vCS");
        ENTRY(NVML_BRAND_NVIDIA_VWS, "vWS");
        ENTRY(NVML_BRAND_NVIDIA_CLOUD_GAMING, "Cloud Gaming");
        ENTRY(NVML_BRAND_QUADRO_RTX, "Quadro");
        ENTRY(NVML_BRAND_NVIDIA_RTX, "NVIDIA");
        ENTRY(NVML_BRAND_NVIDIA, "NVIDIA");
        ENTRY(NVML_BRAND_GEFORCE_RTX, "Geforce RTX");
        ENTRY(NVML_BRAND_TITAN_RTX, "Titan RTX");
    }
#undef ENTRY

    nvRetValue = _nvmlDeviceGetArchitecture(handle, &deviceArch);
    char* archName = "";
#define ENTRY(type, desc) case type: archName = desc; break;
    switch (deviceArch)
    {
        ENTRY(NVML_DEVICE_ARCH_KEPLER, "Kepler");
        ENTRY(NVML_DEVICE_ARCH_MAXWELL, "Maxwell");
        ENTRY(NVML_DEVICE_ARCH_PASCAL, "Pascal");
        ENTRY(NVML_DEVICE_ARCH_VOLTA, "Volta");
        ENTRY(NVML_DEVICE_ARCH_TURING, "Turing");
        ENTRY(NVML_DEVICE_ARCH_AMPERE, "Ampere");
        ENTRY(NVML_DEVICE_ARCH_ADA, "Ada");
        ENTRY(NVML_DEVICE_ARCH_HOPPER, "Hopper");
    }
#undef ENTRY


    printf("\t%s", archName);
    printf("\t%s", brandName);
    printf("\t%s", cDevicename);

    printf("\n");

    return 0;
}

int NvidiaInfo::update()
{
    nvmlReturn_t nvRetValue = NVML_SUCCESS;
    nvmlUtilization_t nvUtilData = {};

    // SM and MEM
    {
        // NOTE: nvUtil.memory is the memory controller utilization not the frame buffer utilization
        nvRetValue = _nvmlDeviceGetUtilizationRates(handle, &nvUtilData);
        if (nvRetValue == NVML_ERROR_NOT_SUPPORTED)
        {
            bGPUUtilSupported = false;
        }
        else if (nvRetValue == NVML_ERROR_UNINITIALIZED)
        {
#if 0
            nvidia_cleanup();
            nvidia_setup();
#endif
        }
        //else CHECK_NVML(nvRetValue, nvmlDeviceGetUtilizationRates);
        metrics.addMetric(METRIC_SM_SOL, nvUtilData.gpu);
        metrics.addMetric(METRIC_MEM_SOL, nvUtilData.memory);
    }

    // Get the GPU frame buffer memory information
    nvmlMemory_t GPUmemoryInfo = {};
    nvRetValue = _nvmlDeviceGetMemoryInfo(handle, &GPUmemoryInfo);
    CHECK_NVML(nvRetValue, nvmlDeviceGetMemoryInfo);

    // verify that the uint64_t to unsigned long cast will not result in lost data
    if (ULLONG_MAX < GPUmemoryInfo.total)
    {
        printf("ERROR: GPU memory size exceeds variable limit\n");
        _nvmlShutdown();
        return NVML_ERROR_NOT_SUPPORTED;
    }

    // convert the frame buffer value to KBytes
    uint64_t ulFrameBufferTotalMBytes = (uint64_t)(GPUmemoryInfo.total / 1024L / 1024L);
    uint64_t ulFrameBufferUsedMBytes = (uint64_t)(ulFrameBufferTotalMBytes - (GPUmemoryInfo.free / 1024L / 1024L));

    // calculate the frame buffer memory utilization

    metrics.addMetric(METRIC_FB_USAGE, ulFrameBufferUsedMBytes * 100.0f / ulFrameBufferTotalMBytes);

    // power and temprature
    {
        uint32_t temp = 0;
        nvRetValue = _nvmlDeviceGetTemperature(handle, NVML_TEMPERATURE_GPU, &temp);
        CHECK_NVML(nvRetValue, nvmlDeviceGetTemperature);
        metrics.addMetric(METRIC_GPU_TEMPERATURE, temp);

        uint32_t power = 0;
        nvRetValue = _nvmlDeviceGetPowerUsage(handle, &power);
        CHECK_NVML(nvRetValue, nvmlDeviceGetPowerUsage);
        metrics.addMetric(METRIC_GPU_POWER, power * 0.001f);

#if 0
        // BUG? fanSpeed is always 0
        uint32_t fanSpeed = 0;
        nvRetValue = _nvmlDeviceGetFanSpeed(handle, &fanSpeed);
        CHECK_NVML(nvRetValue, nvmlDeviceGetFanSpeed);
#endif
    }

    // Get the video encoder utilization (where supported)
    uint32_t uiVidEncoderUtil = 0u;
    uint32_t uiVideEncoderLastSample = 0u;
    nvRetValue = _nvmlDeviceGetEncoderUtilization(handle, &uiVidEncoderUtil, &uiVideEncoderLastSample);
    if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
    {
        bEncoderUtilSupported = false;
    }
    else CHECK_NVML(nvRetValue, nvmlDeviceGetEncoderUtilization);

    // Get the video decoder utilization (where supported)
    uint32_t uiVidDecoderUtil = 0u;
    uint32_t uiVidDecoderLastSample = 0u;
    nvRetValue = _nvmlDeviceGetDecoderUtilization(handle, &uiVidDecoderUtil, &uiVidDecoderLastSample);
    if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
    {
        bDecoderUtilSupported = false;
    }
    else CHECK_NVML(nvRetValue, nvmlDeviceGetEncoderUtilization);

    metrics.addMetric(METRIC_NVENC_SOL, uiVidEncoderUtil);
    metrics.addMetric(METRIC_NVDEC_SOL, uiVidDecoderUtil);

    // Clock
    uint32_t clocks[NVML_CLOCK_COUNT] = {};
    for (int i = 0; i < NVML_CLOCK_COUNT; i++)
    {
        nvRetValue = _nvmlDeviceGetClockInfo(handle, nvmlClockType_t(i), clocks + i);
        if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
        {

        }
        else CHECK_NVML(nvRetValue, nvmlDeviceGetClockInfo);
    }

    // pcie traffic
    uint32_t pcieUtils[NVML_PCIE_UTIL_COUNT] = {};
    uint32_t pcieUtilSum = 0;
    for (int i = 0; i < NVML_PCIE_UTIL_COUNT; i++)
    {
        nvRetValue = _nvmlDeviceGetPcieThroughput(handle, nvmlPcieUtilCounter_t(i), pcieUtils + i);
        if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
        {

        }
        else CHECK_NVML(nvRetValue, nvmlDeviceGetPcieThroughput);

        pcieUtilSum += pcieUtils[i];
    }
    float sol = pcieUtilSum * 0.1 / (pcieCurrentSpeed + 0.1f);
    metrics.addMetric(METRIC_PCIE_SOL, sol);

    // Output the utilization results depending on which of the counters has data available
    // I have opted to display "-" to denote an unsupported value rather than simply display "0"
    // to clarify that the GPU/driver does not support the query. 
    printf("%d %s", deviceId, bMonitorConnected ? "<-" : "");

    if (bGPUUtilSupported) printf("\t%d\t%d", nvUtilData.gpu, nvUtilData.memory);
    else printf("\t-\t-");
    printf("\t%lld / %lld", ulFrameBufferUsedMBytes, ulFrameBufferTotalMBytes);
    printf("\t%-5d\t%-6d", clocks[NVML_CLOCK_SM], clocks[NVML_CLOCK_MEM]);
    printf("\t%-6d\t%-6d", pcieUtils[NVML_PCIE_UTIL_TX_BYTES] / 1024L, pcieUtils[NVML_PCIE_UTIL_RX_BYTES] / 1024L);

#if 0
    if (bEncoderUtilSupported) printf("\t%d", uiVidEncoderUtil);
    else printf("\t-");
    if (bDecoderUtilSupported) printf("\t%d", uiVidDecoderUtil);
    else printf("\t-");
#endif
    //if (bNVLinkSupported)
    if (nvlinkActives[0])
    {
        //for (int j = 0; j < info.numLinks; j++)
        int j = 0;
        uint32_t counter = 0;
        uint64_t rxcounter = 0;
        uint64_t txcounter = 0;
        nvRetValue = _nvmlDeviceGetNvLinkUtilizationCounter(handle, j, counter, &rxcounter, &txcounter);
        if (NVML_ERROR_NOT_SUPPORTED == nvRetValue)
        {
        }
        else
            CHECK_NVML(nvRetValue, nvmlDeviceGetNvLinkUtilizationCounter);
        rxcounter /= 1024L;
        txcounter /= 1024L;
        printf("\t%-5d\t%-5d", txcounter, rxcounter);
        metrics.addMetric(METRIC_NVLINK_TX, txcounter);
        metrics.addMetric(METRIC_NVLINK_RX, rxcounter);
    }

    updatePerProcessInfo();

    return 0;
}

int NvidiaInfo::updatePerProcessInfo()
{
    nvmlReturn_t ret;
#if 0
    // nvmlDeviceGetGraphicsRunningProcesses and nvmlDeviceGetComputeRunningProcesses gives wrong results
    {
        unsigned int infoCount = 0;
        ret = _nvmlDeviceGetGraphicsRunningProcesses(handle, &infoCount, nullptr);
        if (ret == NVML_ERROR_INSUFFICIENT_SIZE || ret == NVML_SUCCESS)
        {
            vector<nvmlProcInfo_t> infos(infoCount);
            ret = _nvmlDeviceGetGraphicsRunningProcesses(handle, &infoCount, infos.data());
            int a = 0;
        }
    }
    {
        unsigned int infoCount = 0;
        ret = _nvmlDeviceGetComputeRunningProcesses(handle, &infoCount, nullptr);
        if (ret == NVML_ERROR_INSUFFICIENT_SIZE || ret == NVML_SUCCESS)
        {
            vector<nvmlProcInfo_t> infos(infoCount);
            ret = _nvmlDeviceGetComputeRunningProcesses(handle, &infoCount, infos.data());
            int a = 0;
        }
    }
#endif
    nvmlEnableState_t mode;
    ret = _nvmlDeviceGetAccountingMode(handle, &mode);
    if (mode == NVML_FEATURE_DISABLED)
        return ret;

    ProcInfos.clear();
    unsigned int pidCount = 0;
    ret = _nvmlDeviceGetAccountingPids(handle, &pidCount, nullptr);
    if (pidCount > 0)
    {
        vector<unsigned int> pids(pidCount);
        ret = _nvmlDeviceGetAccountingPids(handle, &pidCount, pids.data());
        CHECK_NVML(ret, nvmlDeviceGetAccountingPids);
        for (auto pid : pids)
        {
            nvmlAccountingStats_t gpuStats;
            ret = _nvmlDeviceGetAccountingStats(handle, pid, &gpuStats);
            CHECK_NVML(ret, nvmlDeviceGetAccountingStats);
            if (gpuStats.isRunning && (gpuStats.gpuUtilization > 0 || gpuStats.memoryUtilization > 0))
            {
#if 0
                char name[80];
                nvmlSystemGetProcessName(pid, name, 80);
#endif
                auto cpuStats = getEntryFromPID(pid);
                auto p = ProcInfo();
                p.pid = pid;
                p.exeName = cpuStats.szExeFile;
                p.cpuStats = cpuStats;
                p.gpuStats = gpuStats;
                ProcInfos.emplace_back(p);
            }
        }
    }

    return 0;
}

void NvidiaInfo::draw(bool show_legends)
{
    CImg<unsigned char> img(window->width(), window->height(), 1, 3, 50);
    img.draw_grid(-50 * 100.0f / window->width(), -50 * 100.0f / 256, 0, 0, false, true, colors[0], 0.2f, 0xCCCCCCCC, 0xCCCCCCCC);

    metrics.draw(window, img, METRIC_SM_SOL, METRIC_NVDEC_SOL, show_legends);

    // per process info
    if (show_legends)
    {
        int k = 0;
        for (const auto& p : ProcInfos)
        {
            img.draw_text(100, FONT_HEIGHT * (k + 1),
                "%s (%d): %d%% | %d%% \n",
                colors[9], 0, 1, FONT_HEIGHT,
                p.exeName.c_str(), p.pid, p.gpuStats.gpuUtilization, p.gpuStats.memoryUtilization);
            k++;
        }
    }
    img.display(*window);
}

// TODO
static vector<NvidiaInfo> NvidiaInfos;
extern vector<shared_ptr<CImgDisplay>> windows;
extern bool isCimgVisible;
uint32_t uiNumGPUs = 0;


int nvidia_setup()
{
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
    const bool initializeNvPerfResult = InitializeNvPerf();
    if (initializeNvPerfResult)
    {
        const bool openGLLoadDriverResult = OpenGLLoadDriver(); // device periodic sampler requires at least one driver to be loaded
        if (initializeNvPerfResult)
        {
            nsightDeviceIndex = GetCompatibleGpuDeviceIndex();
            if (nsightDeviceIndex == size_t(~0))
            {
                printf("Current device is unsupported, test is skipped.\n");
                return 1;
            }

            const size_t MaxNumUndecodedSamples = 1024;
            const size_t RecordBufferSize = 256 * 1024 * 1024; // 256 MB
        }
    }
#endif
    nvmlReturn_t nvRetValue = NVML_ERROR_UNINITIALIZED;

    if (!LoadNVML())
        return -1;

    // Before any of the NVML functions can be used nvmlInit() must be called
    nvRetValue = _nvmlInitWithFlags(0);
    nvRetValue = _nvmlInit_v2();

    if (NVML_SUCCESS != nvRetValue)
    {
        // Can not call the NVML specific error string handler if the initialization failed
        printf("[%s] error code :%d\n", "nvmlInit", nvRetValue);
        return -1;
    }

    char driverVersion[80];
    int cudaVersion = 0;
    char nvmlVersion[80];
    nvRetValue = _nvmlSystemGetDriverVersion(driverVersion, 80);
    nvRetValue = _nvmlSystemGetCudaDriverVersion(&cudaVersion);
    nvRetValue = _nvmlSystemGetNVMLVersion(nvmlVersion, 80);
    printf("Driver: %s     CUDA: %d.%d      NVML: %s\n",
        driverVersion,
        NVML_CUDA_DRIVER_VERSION_MAJOR(cudaVersion), NVML_CUDA_DRIVER_VERSION_MINOR(cudaVersion),
        nvmlVersion);
    printf("------------------------------------------------------------\n");

    // Get the number of GPUs
    nvRetValue = _nvmlDeviceGetCount_v2(&uiNumGPUs);
    CHECK_NVML(nvRetValue, nvmlDeviceGetCount);

    // In the case that no GPUs were detected
    if (0 == uiNumGPUs)
    {
        printf("No NVIDIA GPUs were detected.\n");
        _nvmlShutdown();
        return -1;
    }

    bool bNVLinkSupported = false;

    printf("GPU\tMODE\tCORES\tBUS\tPCIe\tGB/s\tARCH\tBRAND\tNAME\n");

    NvidiaInfos.resize(uiNumGPUs);

    for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
    {
        auto& info = NvidiaInfos[iDevIDX];
        info.deviceId = iDevIDX;
        info.setup();
        if (isCimgVisible)
        {
            info.window = make_shared<CImgDisplay>(WINDOW_W, WINDOW_H, info.cDevicename, 3);
            windows.push_back(info.window);
        }
    }
    printf("------------------------------------------------------------\n");

    // Print out a header for the utilization output
    printf("GPU\tSM\tMEM\tFBuffer(MB)\tSM-CLK\tMEM-CLK\tPCIE-TX\tPCIE-RX");
    if (bNVLinkSupported)
        printf("\tNVLK-TX\tNVLK-RX");
    printf("\n");
    printf("#id\t%%\t%%\tUsed / All\tMHz\tMHz\tMB\tMB");
    if (bNVLinkSupported)
        printf("\tMB\tMB");
    printf("\n");

    return 0;
}

int nvidia_update()
{
    nvmlReturn_t nvRetValue = NVML_ERROR_UNINITIALIZED;
    // Iterate through all of the GPUs
    for (uint32_t iDevIDX = 0; iDevIDX < uiNumGPUs; iDevIDX++)
    {
        auto& info = NvidiaInfos[iDevIDX];
        GoToXY(0, iDevIDX + 5 + uiNumGPUs + 2);
        info.update();
    }
    return 0;
}

int nvidia_draw(bool show_legends)
{
    for (auto& info : NvidiaInfos)
    {
        info.draw(show_legends);
    }

    return 0;
}

int nvidia_cleanup()
{
    auto nvRetValue = _nvmlShutdown();

    return nvRetValue;
}

int nvidia_draw_imgui()
{
    for (auto& info : NvidiaInfos)
    {
        info.drawImgui();
    }

    return 0;
}

# GpuProf
Realtime profiler for AMD / NVIDIA / Intel GPUs, currently only supports NVIDIA GPU :)

# Screenshot

A single `NVIDIA GTX 1060` running graphics workloads.

![](https://raw.githubusercontent.com/vinjn/GpuProf/master/doc/gtx1060.jpg)

A single `NVIDIA RTX 2080 Super` running Unreal Editor. Lots of new features has been added in this version.

![](https://raw.githubusercontent.com/vinjn/GpuProf/master/doc/rtx2080s.jpg)

# Enable NVLINK

 nvidia-smi -g 0 -dm 1

# Disable NVLINK

 nvidia-smi -g 0 -dm 0

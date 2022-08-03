@set kernelfile=%LocalAppData%\kernel.etl
@set userfile=%LocalAppData%\user.etl

@set KernelProviders=PROC_THREAD+LOADER+Latency+POWER+DISPATCHER+DISK_IO_INIT+FILE_IO+FILE_IO_INIT+VIRT_ALLOC+MEMINFO

@set SessionName=UIforETWSession
@set TRACE_DXC=802ec45a-1e99-4b83-9920-87c98277ba9d
@SET SCHEDULER_LOG_STATE=%TRACE_DXC%:0x04000000:5
@set DX_Flags=Microsoft-Windows-DxgKrnl:0xFFFF:5+Microsoft-Windows-MediaEngine
@set UserProviders=%DX_Flags%+Microsoft-Windows-Win32k:0xfdffffffefffffff+Multi-MAIN+Multi-FrameRate+Multi-Input+Multi-Worker+Microsoft-Windows-Kernel-Memory:0xE0+Microsoft-Windows-Kernel-Process

xperf.exe ^
 -start "NT Kernel Logger" -on %KernelProviders% ^
 -stackwalk Profile+CSwitch+ReadyThread -buffersize 1024 -minbuffers 1200 -maxbuffers 1200 ^
 -f %kernelfile% ^
 -start %SessionName% -on %UserProviders% -buffersize 1024 -minbuffers 400 -maxbuffers 400 ^
 -f %userfile%
xperf.exe -capturestate %SessionName% %UserProviders%
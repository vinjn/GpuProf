@del %LocalAppData%\UIForETWkernel.etl
@del %LocalAppData%\UIForETWuser.etl
xperf.exe -start "NT Kernel Logger" -on Latency+POWER+DISPATCHER+DISK_IO_INIT+FILE_IO+FILE_IO_INIT+VIRT_ALLOC+MEMINFO -stackwalk Profile+CSwitch+ReadyThread -buffersize 1024 -minbuffers 1200 -maxbuffers 1200 -f "%LocalAppData%\UIForETWkernel.etl" -start UIforETWSession -on Microsoft-Windows-Win32k:0xfdffffffefffffff+Multi-MAIN+Multi-FrameRate+Multi-Input+Multi-Worker+Microsoft-Windows-Kernel-Memory:0xE0+Microsoft-Windows-Kernel-Process+Microsoft-Windows-Kernel-Power+Microsoft-Windows-DxgKrnl:0xFFFF:5+Microsoft-Windows-MediaEngine -buffersize 1024 -minbuffers 400 -maxbuffers 400 -f "%LocalAppData%\UIForETWuser.etl"
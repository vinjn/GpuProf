@setlocal

@set dt=%TIME:~0,2%_%TIME:~3,2%_%TIME:~6,2%
@set TIMESTAMP=%dt: =0%
@set TIMESTAMP=%DATE:/=-% %TIME::=-%
xperf -stop "NT Kernel Logger"
xperf -stop %SessionName%
@rem xperf -stop
xperf -merge "%kernelfile%" "%userfile%" "%~dp0GpuProf-%TIMESTAMP%.etl" -compress
@del %kernelfile%
@del %userfile%
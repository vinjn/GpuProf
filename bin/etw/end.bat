set dt=%TIME:~0,2%_%TIME:~3,2%_%TIME:~6,2%
set TIMESTAMP=%dt: =0%
xperf -stop "NT Kernel Logger"
xperf -stop UIforETWSession
rem xperf -stop
xperf -merge %LocalAppData%\UIForETWuser.etl %LocalAppData%\UIForETWkernel.etl GpuProf-%TIMESTAMP%.etl
del %LocalAppData%\UIForETWuser.etl
del %LocalAppData%\UIForETWkernel.etl
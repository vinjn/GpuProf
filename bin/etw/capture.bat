%~d0
cd %~dp0
@del %LocalAppData%\UIForETWuser.etl
@del %LocalAppData%\UIForETWkernel.etl
call %~dp0begin.bat
timeout /t 1
call %~dp0end.bat
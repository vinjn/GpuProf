set mydate=%date:/=%
set TIMESTAMP=%mydate: =_%
set OUTPUT=GpuProf-%TIMESTAMP%
rmdir /S /Q %OUTPUT%
mkdir %OUTPUT%
set SRC=bin/

robocopy %SRC%/etw %OUTPUT%/etw *.bat
robocopy %SRC%/etw %OUTPUT%/etw *.exe
robocopy %SRC%/etw %OUTPUT%/etw *.dll
rem robocopy %SRC%/webui %OUTPUT%/webui *
rem robocopy %SRC% %OUTPUT% gpuprof-web.bat
robocopy %SRC%/ %OUTPUT% gpuprof.exe
robocopy %SRC%/ %OUTPUT% sudo.exe

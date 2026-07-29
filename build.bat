@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /EHsc /W4 /utf-8 /MT /DUNICODE /D_UNICODE engine\athan_times.cpp src\main.cpp /Fe:AdhanBox.exe /link /SUBSYSTEM:WINDOWS

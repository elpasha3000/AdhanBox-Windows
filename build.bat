@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
rc /nologo /fo build_app.res src\app.rc
if errorlevel 1 (echo RC FAILED & exit /b 1)
cl /nologo /O2 /EHsc /W4 /utf-8 /MT /DUNICODE /D_UNICODE engine\athan_times.cpp src\main.cpp build_app.res /Fe:AdhanBox.exe /link /SUBSYSTEM:WINDOWS
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo BUILD OK

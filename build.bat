@echo off
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
rc /nologo /fo build_app.res src\app.rc
if errorlevel 1 (echo RC FAILED & exit /b 1)
cl /nologo /O2 /EHsc /W4 /utf-8 /MT /DUNICODE /D_UNICODE engine\athan_times.cpp src\main.cpp build_app.res /Fe:AdhanBox.exe /link /SUBSYSTEM:WINDOWS
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo BUILD OK

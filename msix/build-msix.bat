@echo off
REM ============================================================
REM  AdhanBox - Microsoft Store package (MSIX, x64, UNSIGNED)
REM  Microsoft signs the package at publish time - do not sign it here.
REM  ASCII-only file (Arabic breaks cmd parsing).
REM ============================================================
rem مسار Windows SDK — غيّره لو مختلف عندك
if not defined SDK set SDK=%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.22621.0\x64
set ROOT=%~dp0..
set PKG=%ROOT%\msix\pkg

taskkill /F /IM AdhanBox.exe >nul 2>&1
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%ROOT%"

echo ===== BUILD AdhanBox.exe =====
call build.bat >nul
if not exist "%ROOT%\AdhanBox.exe" (echo *** MAIN BUILD FAILED *** & exit /b 1)

echo ===== BUILD AdhanBoxStartup.exe =====
cl /nologo /O2 /EHsc /W4 /utf-8 /MT /DUNICODE /D_UNICODE src\startup.cpp /Fe:AdhanBoxStartup.exe /link /SUBSYSTEM:WINDOWS user32.lib >nul
if not exist "%ROOT%\AdhanBoxStartup.exe" (echo *** LAUNCHER BUILD FAILED *** & exit /b 1)

echo ===== STAGE =====
copy /y "%ROOT%\AdhanBox.exe" "%PKG%\" >nul
copy /y "%ROOT%\AdhanBoxStartup.exe" "%PKG%\" >nul
copy /y "%ROOT%\msix\AppxManifest.xml" "%PKG%\" >nul

echo ===== PACK =====
if exist "%ROOT%\msix\AdhanBox_0.4.0.0_x64.msix" del "%ROOT%\msix\AdhanBox_0.4.0.0_x64.msix"
"%SDK%\makeappx.exe" pack /d "%PKG%" /p "%ROOT%\msix\AdhanBox_0.4.0.0_x64.msix" /o
if errorlevel 1 (echo *** PACK FAILED *** & exit /b 2)

echo ===== DONE =====
dir /b "%ROOT%\msix\AdhanBox_0.4.0.0_x64.msix"

@echo off
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /EHsc /W4 /utf-8 /MT athan_times.cpp test_engine.cpp /Fe:test_engine.exe

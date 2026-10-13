@echo off
cd /d F:\TZcode\armor-piercer
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
echo [1] vcvars ok rc=%ERRORLEVEL%
where cl
rc /fo app.res app.rc
echo [2] rc step rc=%ERRORLEVEL%
cl /nologo /std:c++17 /EHsc /utf-8 /O2 /DUNICODE /D_UNICODE /W3 zcode_armor_piercer.cpp app.res /Fe:piercer.exe /Fo:zcode_armor_piercer.obj /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib
echo [3] cl step rc=%ERRORLEVEL%
if exist piercer.exe echo [OK] build success

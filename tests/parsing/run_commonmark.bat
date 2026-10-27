@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL exit /b 1
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cd /d "%ROOT%"
python "%ROOT%\tools\markdown-conformance\prepare.py" --fetch
if errorlevel 1 exit /b %errorlevel%
cmake -Wno-dev -G Ninja -S "%ROOT%" -B "%ROOT%\build\core"
if errorlevel 1 exit /b %errorlevel%
cmake --build "%ROOT%\build\core" --parallel 4 --target FoliaCommonMarkTests
if errorlevel 1 exit /b %errorlevel%
python "%ROOT%\tools\markdown-conformance\run.py" --executable "%ROOT%\build\core\FoliaCommonMarkTests.exe" %*
exit /b %errorlevel%

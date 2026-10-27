@echo off
setlocal
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL exit /b 1
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cd /d "%ROOT%"
if not exist "%ROOT%\build\markdown-conformance\results.json" exit /b 2
python "%ROOT%\tools\markdown-conformance\prepare.py" --fetch --reference
if errorlevel 1 exit /b %errorlevel%
cmake -Wno-dev -G Ninja -S "%ROOT%" -B "%ROOT%\build\core" -DFOLIA_BUILD_MD4C_REFERENCE_TESTS=ON
if errorlevel 1 exit /b %errorlevel%
cmake --build "%ROOT%\build\core" --parallel 4 --target FoliaMd4cReferenceTests
if errorlevel 1 exit /b %errorlevel%
python "%ROOT%\tools\markdown-conformance\reference.py" --executable "%ROOT%\build\core\FoliaMd4cReferenceTests.exe" %*
exit /b %errorlevel%

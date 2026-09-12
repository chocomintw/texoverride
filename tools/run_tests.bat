@echo off
REM Builds and runs every test in this folder. Exits nonzero if any of them fails, so CI
REM can use it as a gate. Run it after touching the gate, the settings file, the sweep that
REM revalidates slot names, or ctlPath.
cd /d "%~dp0.."

if "%VSCMD_ARG_TGT_ARCH%"=="x64" goto run
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" echo Visual Studio Build Tools not found. && exit /b 1
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -all -latest -products * -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" echo Found Visual Studio but not the C++ tools. && exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

:run
set "OUT=%TEMP%\texoverride-tests"
if not exist "%OUT%" mkdir "%OUT%"
REM One source list for all of them: a test links what it needs and ignores the rest.
set "SRC=src\core\utils.cpp src\core\state.cpp src\core\logger.cpp src\core\settings.cpp src\streaming\gate.cpp"
set "FAILED="

call :one test_gate_keys || set "FAILED=%FAILED% test_gate_keys"
call :one test_slotkey   || set "FAILED=%FAILED% test_slotkey"
call :one test_settings  || set "FAILED=%FAILED% test_settings"
call :one test_reval     || set "FAILED=%FAILED% test_reval"
call :one test_ctlpath   || set "FAILED=%FAILED% test_ctlpath"

del /q "%OUT%\*.obj" 2>nul
if not "%FAILED%"=="" echo. && echo FAILED:%FAILED% && exit /b 1
echo.
echo all tests passed
exit /b 0

:one
cl /nologo /std:c++17 /W4 /EHsc /MT /I src tools\%1.cpp %SRC% /Fe:"%OUT%\%1.exe" /Fo:"%OUT%\\" >nul || exit /b 1
"%OUT%\%1.exe" || exit /b 1
exit /b 0

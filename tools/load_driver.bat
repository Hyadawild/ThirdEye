@echo off
title SecureBridge Driver Loader
color 0A

:: Check for admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] Administrator privileges required
    echo [*] Restarting as admin...
    powershell start -verb runas '%0'
    exit
)

:: Enable test signing
echo [*] Enabling test signing mode...
bcdedit /set testsigning on >nul 2>&1
bcdedit /set nointegritychecks on >nul 2>&1

:menu
cls
echo ========================================
echo    SecureBridge Driver Manager
echo ========================================
echo.
echo [1] Load Driver (Standard)
echo [2] Load Driver (Manual Map)
echo [3] Unload Driver
echo [4] Check Driver Status
echo [5] Test Communication
echo [6] Exit
echo.
set /p choice="Select option: "

if "%choice%"=="1" goto load_standard
if "%choice%"=="2" goto load_manual
if "%choice%"=="3" goto unload
if "%choice%"=="4" goto status
if "%choice%"=="5" goto test
if "%choice%"=="6" goto exit

:load_standard
echo.
echo [*] Copying driver to system directory...
copy /Y SecureBridge.sys C:\Windows\System32\drivers\ >nul 2>&1

echo [*] Creating service...
sc create SecureBridge binPath= C:\Windows\System32\drivers\SecureBridge.sys type= kernel start= demand >nul 2>&1

echo [*] Starting driver...
sc start SecureBridge >nul 2>&1

if %errorLevel% equ 0 (
    echo [^|] Driver loaded successfully!
) else (
    echo [!] Failed to load driver. Error: %errorLevel%
    echo [*] Try manual mapping option instead.
)

pause
goto menu

:load_manual
echo.
echo [*] Manual mapping driver...

:: Check for kdmapper
if not exist "kdmapper.exe" (
    echo [!] kdmapper.exe not found!
    echo [*] Download from: https://github.com/TheCruZ/kdmapper
    pause
    goto menu
)

echo [*] Running kdmapper...
kdmapper.exe SecureBridge.sys

if %errorLevel% equ 0 (
    echo [^|] Driver mapped successfully!
) else (
    echo [!] Mapping failed.
)

pause
goto menu

:unload
echo.
echo [*] Stopping driver...
sc stop SecureBridge >nul 2>&1

echo [*] Deleting service...
sc delete SecureBridge >nul 2>&1

echo [*] Removing driver file...
del /F C:\Windows\System32\drivers\SecureBridge.sys >nul 2>&1

echo [^|] Driver unloaded.
pause
goto menu

:status
echo.
echo [*] Checking driver status...
sc query SecureBridge

echo.
echo [*] Checking device...
dir \\.\SecureBridge 2>nul
if %errorLevel% equ 0 (
    echo [^|] Device exists.
) else (
    echo [!] Device not found.
)

pause
goto menu

:test
echo.
echo [*] Testing driver communication...

:: Create test client if not exists
if not exist "test_client.exe" (
    echo [*] Compiling test client...
    cl /nologo test_client.cpp /Fe:test_client.exe
)

test_client.exe
pause
goto menu

:exit
echo.
echo [*] Exiting...
exit /b 0
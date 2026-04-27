@echo off
title SecureBridge Driver Unloader
color 0C

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Administrator rights required!
    pause
    exit
)

echo ========================================
echo    Unloading SecureBridge Driver
echo ========================================
echo.

echo [*] Stopping service...
sc stop SecureBridge

echo [*] Deleting service...
sc delete SecureBridge

echo [*] Removing driver file...
del /F C:\Windows\System32\drivers\SecureBridge.sys 2>nul

echo [*] Disabling test signing...
bcdedit /set testsigning off >nul 2>&1
bcdedit /set nointegritychecks off >nul 2>&1

echo.
echo [^|] Driver unloaded successfully!
echo [*] Reboot recommended for security settings to take effect.

pause
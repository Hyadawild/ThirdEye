@echo off
title PUBG External ESP
color 0A

echo ========================================
echo    PUBG External ESP Framework
echo    Press DEL to exit
echo ========================================
echo.

:: Check admin rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] This requires Administrator privileges
    echo [*] Restarting as admin...
    powershell start -verb runas '%0'
    exit
)

:: Find Python
where python >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] Python not found. Please install Python 3.8+
    pause
    exit
)

:: Check if driver is loaded
sc query SecureBridge >nul 2>&1
if %errorLevel% neq 0 (
    echo [*] Loading kernel driver...
    sc start SecureBridge
    timeout /t 2 /nobreak >nul
)

:: Launch ESP
echo [*] Starting ESP...
python esp_main.py

:: Cleanup on exit
echo [*] Cleaning up...
sc stop SecureBridge >nul 2>&1

pause
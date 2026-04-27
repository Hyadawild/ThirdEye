# SecureBridge Kernel Driver for PUBG ESP

## Overview
SecureBridge is a kernel-mode driver designed to provide direct memory access to PUBG for external ESP rendering, bypassing BattlEye and other anti-cheat systems.

## Features
- Direct memory read/write via MmCopyVirtualMemory
- DKOM (Direct Kernel Object Manipulation) for hiding
- Anti-cheat callback removal
- No registry traces (manual mapping option)
- Encrypted communication
- Windows 10/11 support

## Prerequisites
- Windows 10/11 x64 (19041+)
- Visual Studio 2022 with C++ tools
- Windows Driver Kit (WDK) 10/11
- Administrative privileges

## Quick Start

### 1. Build Driver
```batch
cd build
build.bat
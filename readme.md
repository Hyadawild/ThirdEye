# 🔒 SecureBridge - Kernel Driver Framework for External Game Memory Access

## 📋 Overview
SecureBridge is a complete kernel-mode driver framework designed for external process memory access, specifically optimized for PUBG ESP (External Box ESP) rendering. The framework operates at Ring0 level, bypassing user-mode anti-cheat hooks through direct kernel memory operations, DKOM (Direct Kernel Object Manipulation), and callback removal techniques.

## ⚠️ DISCLAIMER
**This project is for educational purposes only. The techniques demonstrated are common in security research, reverse engineering, and Windows internals study. Use responsibly and only on systems you own or have explicit permission to test.**

---

## 🏗️ Project Architecture

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ USER APPLICATIONS                                                           │
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐     │
│ │ ESP.py      │ │ test_client │ │ Loader.exe  │ │ Injector.exe        │     │
│ │ (Python)    │ │ (C++)       │ │ (C++)       │ │ (C++)               │     │
│ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────────┬──────────┘     │
│        │               │               │                   │                │
│        └───────────────┼───────────────┼───────────────────┘                │
│                        ▼               ▼                                    │
│                ┌──────────────┐ ┌──────────────┐                            │
│                │SecureBridge  │ │ KDMapper     │                            │
│                │ Client.dll   │ │ (Manual)     │                            │
│                └──────┬───────┘ └──────────────┘                            │
└───────────────────────┼─────────────────────────────────────────────────────┘
                        │ DeviceIoControl
                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ KERNEL LAYER (Ring0)                                                        │
│ ┌─────────────────────────────────────────────────────────────────────┐     │
│ │ SecureBridge.sys (Kernel Driver)                                    │     │
│ │ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌────────────┐      │     │
│ │ │ MemoryOps   │ │ DKOM        │ │ Callback    │ │ Anti-      │      │     │
│ │ │ R/W via     │ │ Hiding      │ │ Cleaner     │ │ Analysis   │      │     │
│ │ │ MDL         │ │ Techniques  │ │             │ │            │      │     │
│ │ └─────────────┘ └─────────────┘ └─────────────┘ └────────────┘      │     │
│ └─────────────────────────────────────────────────────────────────────┘     │
│                                 │                                           │
│                                 │ MmCopyVirtualMemory                       │
│                                 ▼                                           │
│ ┌─────────────────────────────────────────────────────────────────────┐     │
│ │ Target Process (PUBG)                                               │     │
│ │ TslGame.exe - Game Memory (Entity Lists, Positions, View Matrix)    │     │
│ └─────────────────────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────────────┘

## File Structure
SecureBridge/
│
├── 📁 driver/               # Kernel Driver Source (Ring0)
│   ├── driver_main.cpp      # Driver entry point & initialization
│   ├── memory_ops.cpp       # Memory read/write via MmCopyVirtualMemory
│   ├── dkom.cpp             # DKOM hiding techniques
│   ├── anti_analysis.cpp    # Anti-debug and anti-analysis
│   ├── communication.cpp    # IOCTL handling & dispatch
│   ├── hide_driver.cpp      # Driver hiding from module lists
│   ├── callback_cleaner.cpp # Anti-cheat callback removal
│   ├── physical_read.cpp    # Physical memory access
│   ├── pattern_scan.cpp     # Pattern scanning for offsets
│   ├── offset_resolver.cpp  # Windows version offset resolution
│   ├── driver.h             # Main headers & definitions
│   ├── sources              # WDK build configuration
│   └── makefile             # WDK makefile
│
├── 📁 client/               # User Mode Client DLL
│   ├── SecureBridgeClient.cpp # Client DLL implementation
│   ├── SecureBridgeClient.h   # Client headers
│   └── client.def           # DLL export definitions
│
├── 📁 esp/                  # External ESP (Python)
│   ├── esp_main.py          # Main ESP entry point
│   ├── overlay.py           # DirectX 11 overlay renderer
│   ├── world_to_screen.py   # 3D to 2D projection
│   ├── memory_reader.py     # Driver communication layer
│   ├── offsets.json         # Current PUBG offsets
│   ├── config.json          # User configuration
│   └── requirements.txt     # Python dependencies
│
├── 📁 loader/               # Driver Loaders & Injectors
│   ├── 📁 kdmapper/
│   │   ├── kdmapper.cpp     # Manual kernel mapper
│   │   └── build_kdmapper.bat # KDMapper build script
│   ├── 📁 manual_map/
│   │   └── manual_map.cpp   # Manual DLL mapper
│   ├── 📁 reflective_loader/
│   │   ├── reflective_loader.c # Reflective loader stub
│   │   └── reflect_loader.h    # Header
│   ├── 📁 process_hollow/
│   │   └── process_hollow.cpp  # Process hollowing tool
│   ├── loader_main.cpp      # Main loader UI
│   ├── load_driver_standard.bat # Standard service loader
│   ├── load_vulnerable.bat  # Vulnerable driver loader
│   ├── test_client.cpp      # Driver test utility
│   └── README_LOADER.md     # Loader documentation
│
├── 📁 tools/                # Utility Tools
│   ├── dump_offsets.py      # Offset dumper (IDA script)
│   ├── sig_scanner.py       # Pattern scanner
│   ├── sign_driver.bat      # Driver signing utility
│   └── clear_events.bat     # Windows event log cleaner
│
├── 📁 build/                # Build Output
│   ├── build.bat            # Complete build script
│   ├── clean.bat            # Clean build artifacts
│   ├── SecureBridge.sys     # Compiled kernel driver
│   ├── SecureBridgeClient.dll # Compiled client DLL
│   └── test_client.exe      # Compiled test utility
│
└── 📄 README.md             # This file

## Prerequisites
Windows Requirements
Windows 10/11 x64 (Build 19041+)

Administrative privileges

10GB free disk space for build tools

16GB RAM recommended for VM testing
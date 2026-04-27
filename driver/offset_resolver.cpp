#include "driver.h"

typedef struct _WIN_OFFSETS {
    DWORD BuildNumber;
    DWORD ActiveProcessLinksOffset;
    DWORD PEBOffset;
    DWORD ProcessIdOffset;
    DWORD ThreadListHeadOffset;
} WIN_OFFSETS, *PWIN_OFFSETS;

// Offsets for different Windows versions
WIN_OFFSETS g_Offsets[] = {
    {19041, 0x448, 0x550, 0x440, 0x5A0},  // Windows 10 20H1
    {19042, 0x448, 0x550, 0x440, 0x5A0},  // Windows 10 20H2
    {19043, 0x448, 0x550, 0x440, 0x5A0},  // Windows 10 21H1
    {19044, 0x448, 0x550, 0x440, 0x5A0},  // Windows 10 21H2
    {19045, 0x448, 0x550, 0x440, 0x5A0},  // Windows 10 22H2
    {22000, 0x448, 0x550, 0x440, 0x5A0},  // Windows 11 21H2
    {22621, 0x448, 0x550, 0x440, 0x5A0},  // Windows 11 22H2
    {22631, 0x448, 0x550, 0x440, 0x5A0},  // Windows 11 23H2
    {0, 0, 0, 0, 0}  // Terminator
};

// Get current Windows build number
DWORD GetCurrentBuildNumber() {
    RTL_OSVERSIONINFOW VersionInfo = {0};
    VersionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
    
    RtlGetVersion(&VersionInfo);
    
    return VersionInfo.dwBuildNumber;
}

// Get EPROCESS offset for current Windows version
DWORD GetActiveProcessLinksOffset() {
    DWORD BuildNumber = GetCurrentBuildNumber();
    DWORD i = 0;
    
    while (g_Offsets[i].BuildNumber != 0) {
        if (BuildNumber >= g_Offsets[i].BuildNumber) {
            return g_Offsets[i].ActiveProcessLinksOffset;
        }
        i++;
    }
    
    // Default fallback
    return 0x448;
}

// Get PEB offset
DWORD GetPebOffset() {
    DWORD BuildNumber = GetCurrentBuildNumber();
    DWORD i = 0;
    
    while (g_Offsets[i].BuildNumber != 0) {
        if (BuildNumber >= g_Offsets[i].BuildNumber) {
            return g_Offsets[i].PEBOffset;
        }
        i++;
    }
    
    return 0x550;
}

// Get PID offset in EPROCESS
DWORD GetProcessIdOffset() {
    DWORD BuildNumber = GetCurrentBuildNumber();
    DWORD i = 0;
    
    while (g_Offsets[i].BuildNumber != 0) {
        if (BuildNumber >= g_Offsets[i].BuildNumber) {
            return g_Offsets[i].ProcessIdOffset;
        }
        i++;
    }
    
    return 0x440;
}
#include "SecureBridgeClient.h"
#include <winternl.h>
#include <iostream>

#pragma comment(lib, "ntdll.lib")

#define IOCTL_READ_MEMORY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HIDE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct _SecureRequest {
    DWORD Magic;
    HANDLE ProcessId;
    ULONG_PTR Address;
    PVOID Buffer;
    SIZE_T Size;
    DWORD Checksum;
} SecureRequest, *PSecureRequest;

typedef struct _ModuleRequest {
    HANDLE ProcessId;
    CHAR ModuleName[64];
    ULONG_PTR BaseAddress;
    SIZE_T ModuleSize;
} ModuleRequest, *PModuleRequest;
#pragma pack(pop)

static HANDLE g_hDevice = INVALID_HANDLE_VALUE;
static DWORD g_LastError = 0;

// Calculate checksum
DWORD CalculateChecksum(PVOID Data, SIZE_T Size) {
    DWORD Checksum = 0;
    PBYTE Bytes = (PBYTE)Data;
    
    for (SIZE_T i = 0; i < Size; i++) {
        Checksum += Bytes[i];
    }
    
    return Checksum ^ 0xFFFFFFFF;
}

// Connect to driver
SECUREBRIDGE_API BOOL InitializeDriver() {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        return TRUE;
    }
    
    // Try multiple device names (randomized for evasion)
    const wchar_t* DeviceNames[] = {
        L"\\\\.\\SecureBridge",
        L"\\\\.\\ProtectedBridge",
        L"\\\\.\\KernelComms",
        L"\\\\.\\HiddenDevice",
        L"\\\\.\\SystemMapper"
    };
    
    for (int i = 0; i < 5; i++) {
        g_hDevice = CreateFileW(DeviceNames[i], 
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL);
        
        if (g_hDevice != INVALID_HANDLE_VALUE) {
            return TRUE;
        }
    }
    
    g_LastError = GetLastError();
    return FALSE;
}

// Cleanup driver connection
SECUREBRIDGE_API VOID CleanupDriver() {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDevice);
        g_hDevice = INVALID_HANDLE_VALUE;
    }
}

// Read memory through driver
SECUREBRIDGE_API BOOL ReadProcessMemorySecure(DWORD ProcessId, ULONG_PTR Address, PVOID Buffer, SIZE_T Size) {
    if (!InitializeDriver()) {
        return FALSE;
    }
    
    SecureRequest Request = {0};
    Request.Magic = 0xDEADBEEF;
    Request.ProcessId = (HANDLE)ProcessId;
    Request.Address = Address;
    Request.Buffer = Buffer;
    Request.Size = Size;
    Request.Checksum = CalculateChecksum(&Request.ProcessId, 
                                         sizeof(Request.ProcessId) + 
                                         sizeof(Request.Address) + 
                                         sizeof(Request.Size));
    
    DWORD BytesReturned = 0;
    BOOL Result = DeviceIoControl(g_hDevice, IOCTL_READ_MEMORY,
                                  &Request, sizeof(Request),
                                  Buffer, (DWORD)Size,
                                  &BytesReturned, NULL);
    
    if (!Result) {
        g_LastError = GetLastError();
    }
    
    return Result;
}

// Write memory through driver
SECUREBRIDGE_API BOOL WriteProcessMemorySecure(DWORD ProcessId, ULONG_PTR Address, PVOID Buffer, SIZE_T Size) {
    if (!InitializeDriver()) {
        return FALSE;
    }
    
    SecureRequest Request = {0};
    Request.Magic = 0xDEADBEEF;
    Request.ProcessId = (HANDLE)ProcessId;
    Request.Address = Address;
    Request.Buffer = Buffer;
    Request.Size = Size;
    Request.Checksum = CalculateChecksum(&Request.ProcessId, 
                                         sizeof(Request.ProcessId) + 
                                         sizeof(Request.Address) + 
                                         sizeof(Request.Size));
    
    DWORD BytesReturned = 0;
    BOOL Result = DeviceIoControl(g_hDevice, IOCTL_WRITE_MEMORY,
                                  &Request, sizeof(Request),
                                  NULL, 0,
                                  &BytesReturned, NULL);
    
    if (!Result) {
        g_LastError = GetLastError();
    }
    
    return Result;
}

// Get module base address
SECUREBRIDGE_API ULONG_PTR GetModuleBaseSecure(DWORD ProcessId, LPCWSTR ModuleName) {
    if (!InitializeDriver()) {
        return 0;
    }
    
    ModuleRequest Request = {0};
    Request.ProcessId = (HANDLE)ProcessId;
    
    // Convert wide to narrow
    size_t Converted = 0;
    wcstombs_s(&Converted, Request.ModuleName, 63, ModuleName, 63);
    
    DWORD BytesReturned = 0;
    if (DeviceIoControl(g_hDevice, IOCTL_GET_MODULE,
                        &Request, sizeof(Request),
                        &Request, sizeof(Request),
                        &BytesReturned, NULL)) {
        return Request.BaseAddress;
    }
    
    g_LastError = GetLastError();
    return 0;
}

// Hide process from anti-cheat
SECUREBRIDGE_API BOOL HideProcessSecure(DWORD ProcessId) {
    if (!InitializeDriver()) {
        return FALSE;
    }
    
    DWORD BytesReturned = 0;
    BOOL Result = DeviceIoControl(g_hDevice, IOCTL_HIDE_PROCESS,
                                  &ProcessId, sizeof(ProcessId),
                                  NULL, 0,
                                  &BytesReturned, NULL);
    
    if (!Result) {
        g_LastError = GetLastError();
    }
    
    return Result;
}

// Get last error
SECUREBRIDGE_API DWORD GetLastDriverError() {
    return g_LastError;
}
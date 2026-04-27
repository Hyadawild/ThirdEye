#pragma once
#include <windows.h>

#ifdef SECUREBRIDGECLIENT_EXPORTS
#define SECUREBRIDGE_API __declspec(dllexport)
#else
#define SECUREBRIDGE_API __declspec(dllimport)
#endif

typedef struct _MemoryRequest {
    DWORD ProcessId;
    ULONG_PTR Address;
    PVOID Buffer;
    SIZE_T Size;
} MemoryRequest, *PMemoryRequest;

extern "C" {
    SECUREBRIDGE_API BOOL InitializeDriver();
    SECUREBRIDGE_API VOID CleanupDriver();
    SECUREBRIDGE_API BOOL ReadProcessMemorySecure(DWORD ProcessId, ULONG_PTR Address, PVOID Buffer, SIZE_T Size);
    SECUREBRIDGE_API BOOL WriteProcessMemorySecure(DWORD ProcessId, ULONG_PTR Address, PVOID Buffer, SIZE_T Size);
    SECUREBRIDGE_API ULONG_PTR GetModuleBaseSecure(DWORD ProcessId, LPCWSTR ModuleName);
    SECUREBRIDGE_API BOOL HideProcessSecure(DWORD ProcessId);
    SECUREBRIDGE_API DWORD GetLastDriverError();
}
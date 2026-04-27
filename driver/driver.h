#pragma once
#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>
#include <ntimage.h>
#include <intrin.h>

#define DRIVER_POOL_TAG 'pUgB'
#define DRIVER_DEVICE_NAME L"\\Device\\SecureBridge"
#define DRIVER_SYMBOLIC_LINK L"\\DosDevices\\SecureBridge"

// IOCTL Codes
#define IOCTL_READ_MEMORY      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PROTECT_PROCESS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HIDE_PROCESS     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Secure request structure
#pragma pack(push, 1)
typedef struct _SECURE_REQUEST {
    ULONG   Magic;              // 0xDEADBEEF
    HANDLE  ProcessId;
    ULONG_PTR Address;
    PVOID   Buffer;
    SIZE_T  Size;
    ULONG   Checksum;
} SECURE_REQUEST, *PSECURE_REQUEST;

typedef struct _MODULE_REQUEST {
    HANDLE  ProcessId;
    CHAR    ModuleName[64];
    ULONG_PTR BaseAddress;
    SIZE_T  ModuleSize;
} MODULE_REQUEST, *PMODULE_REQUEST;
#pragma pack(pop)

// Function declarations
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// Memory operations
NTSTATUS ReadKernelMemory(HANDLE Pid, ULONG_PTR Address, PVOID Buffer, SIZE_T Size, PSIZE_T BytesRead);
NTSTATUS WriteKernelMemory(HANDLE Pid, ULONG_PTR Address, PVOID Buffer, SIZE_T Size, PSIZE_T BytesWritten);
NTSTATUS GetModuleBase(HANDLE Pid, PCHAR ModuleName, PULONG_PTR BaseAddress, PSIZE_T ModuleSize);

// DKOM operations
VOID HideProcess(HANDLE Pid);
VOID HideDriver(PDRIVER_OBJECT DriverObject);

// Anti-analysis
VOID AntiDebugCheck();
VOID ClearCallbacks();
VOID DisableProtection();

// Utility
ULONG CalculateChecksum(PVOID Data, SIZE_T Size);
BOOLEAN ValidateRequest(PSECURE_REQUEST Request);
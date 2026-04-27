#include "driver.h"

PDEVICE_OBJECT g_DeviceObject = NULL;
PDRIVER_OBJECT g_DriverObject = NULL;

// Callback registration handles
PVOID g_RegistryCallback = NULL;
PVOID g_ProcessCallback = NULL;
PVOID g_ImageCallback = NULL;

// Hidden process list
typedef struct _HIDDEN_PROCESS {
    LIST_ENTRY List;
    HANDLE Pid;
} HIDDEN_PROCESS, *PHIDDEN_PROCESS;

LIST_ENTRY g_HiddenProcesses;
ERESOURCE g_HiddenProcessLock;

// Driver entry point
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    NTSTATUS Status = STATUS_SUCCESS;
    UNICODE_STRING DeviceName, SymbolicLink;
    PDEVICE_OBJECT DeviceObject = NULL;
    
    DbgPrint("[SecureBridge] Driver loading...\n");
    
    // Store driver object
    g_DriverObject = DriverObject;
    
    // Initialize hidden process list
    InitializeListHead(&g_HiddenProcesses);
    ExInitializeResourceLite(&g_HiddenProcessLock);
    
    // Create device object
    RtlInitUnicodeString(&DeviceName, DRIVER_DEVICE_NAME);
    Status = IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 
                            FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceObject);
    
    if (!NT_SUCCESS(Status)) {
        DbgPrint("[SecureBridge] Failed to create device: 0x%X\n", Status);
        return Status;
    }
    
    g_DeviceObject = DeviceObject;
    DeviceObject->Flags |= DO_DIRECT_IO;
    DeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
    
    // Create symbolic link
    RtlInitUnicodeString(&SymbolicLink, DRIVER_SYMBOLIC_LINK);
    Status = IoCreateSymbolicLink(&SymbolicLink, &DeviceName);
    
    if (!NT_SUCCESS(Status)) {
        IoDeleteDevice(DeviceObject);
        DbgPrint("[SecureBridge] Failed to create symbolic link: 0x%X\n", Status);
        return Status;
    }
    
    // Setup dispatch functions
    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = DeviceControl;
    }
    
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceControl;
    DriverObject->DriverUnload = DriverUnload;
    
    // Hide driver from detection
    HideDriver(DriverObject);
    
    // Clear anti-cheat callbacks
    ClearCallbacks();
    
    // Disable protection mechanisms
    DisableProtection();
    
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
    DbgPrint("[SecureBridge] Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

// Driver unload
VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING SymbolicLink;
    
    DbgPrint("[SecureBridge] Driver unloading...\n");
    
    // Clean up callbacks
    if (g_RegistryCallback) {
        CmUnRegisterCallback(g_RegistryCallback);
    }
    
    if (g_ProcessCallback) {
        PsSetCreateProcessNotifyRoutineEx(g_ProcessCallback, TRUE);
    }
    
    if (g_ImageCallback) {
        PsRemoveLoadImageNotifyRoutine(g_ImageCallback);
    }
    
    // Clean up hidden process list
    ExDeleteResourceLite(&g_HiddenProcessLock);
    
    // Delete symbolic link and device
    RtlInitUnicodeString(&SymbolicLink, DRIVER_SYMBOLIC_LINK);
    IoDeleteSymbolicLink(&SymbolicLink);
    
    if (g_DeviceObject) {
        IoDeleteDevice(g_DeviceObject);
    }
    
    DbgPrint("[SecureBridge] Driver unloaded\n");
}
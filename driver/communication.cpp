#include "driver.h"

extern PDEVICE_OBJECT g_DeviceObject;

// Calculate simple checksum
ULONG CalculateChecksum(PVOID Data, SIZE_T Size) {
    ULONG Checksum = 0;
    PUCHAR ByteData = (PUCHAR)Data;
    
    for (SIZE_T i = 0; i < Size; i++) {
        Checksum += ByteData[i];
    }
    
    return Checksum ^ 0xFFFFFFFF;
}

// Validate request integrity
BOOLEAN ValidateRequest(PSECURE_REQUEST Request) {
    if (!Request) {
        return FALSE;
    }
    
    // Check magic number
    if (Request->Magic != 0xDEADBEEF) {
        return FALSE;
    }
    
    // Validate buffer ranges
    if (Request->Buffer && !MmIsAddressValid(Request->Buffer)) {
        return FALSE;
    }
    
    // Validate checksum
    ULONG CalcChecksum = CalculateChecksum(&Request->ProcessId, 
                           sizeof(Request->ProcessId) + 
                           sizeof(Request->Address) + 
                           sizeof(Request->Size));
    
    return (CalcChecksum == Request->Checksum);
}

// IRP Dispatch
NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG_PTR Information = 0;
    
    // Anti-debug each dispatch
    AntiDebugCheck();
    
    // Process IOCTL
    switch (Stack->Parameters.DeviceIoControl.IoControlCode) {
        
        case IOCTL_READ_MEMORY: {
            PSECURE_REQUEST Request = (PSECURE_REQUEST)Irp->AssociatedIrp.SystemBuffer;
            
            if (ValidateRequest(Request)) {
                SIZE_T BytesRead = 0;
                Status = ReadKernelMemory(Request->ProcessId, Request->Address, 
                                         Request->Buffer, Request->Size, &BytesRead);
                Information = BytesRead;
                
                // Clear sensitive data
                Request->Magic = 0;
                Request->Checksum = 0;
            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        
        case IOCTL_WRITE_MEMORY: {
            PSECURE_REQUEST Request = (PSECURE_REQUEST)Irp->AssociatedIrp.SystemBuffer;
            
            if (ValidateRequest(Request)) {
                SIZE_T BytesWritten = 0;
                Status = WriteKernelMemory(Request->ProcessId, Request->Address,
                                          Request->Buffer, Request->Size, &BytesWritten);
                Information = BytesWritten;
                
                Request->Magic = 0;
                Request->Checksum = 0;
            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        
        case IOCTL_HIDE_PROCESS: {
            HANDLE Pid = *(HANDLE*)Irp->AssociatedIrp.SystemBuffer;
            HideProcess(Pid);
            Status = STATUS_SUCCESS;
            break;
        }
        
        default:
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return Status;
}
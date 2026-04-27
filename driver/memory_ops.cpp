#include "driver.h"

// Read memory directly via physical mapping
NTSTATUS ReadKernelMemory(HANDLE Pid, ULONG_PTR Address, PVOID Buffer, SIZE_T Size, PSIZE_T BytesRead) {
    PEPROCESS TargetProcess = NULL;
    NTSTATUS Status = STATUS_SUCCESS;
    SIZE_T ReturnSize = 0;
    
    if (!Buffer || !Size || !Address) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Get EPROCESS from PID
    Status = PsLookupProcessByProcessId(Pid, &TargetProcess);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    __try {
        // Use MmCopyVirtualMemory (bypasses hooks)
        Status = MmCopyVirtualMemory(TargetProcess, (PVOID)Address, 
                                     PsGetCurrentProcess(), Buffer, 
                                     Size, KernelMode, &ReturnSize);
        
        if (NT_SUCCESS(Status) && BytesRead) {
            *BytesRead = ReturnSize;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        Status = STATUS_ACCESS_VIOLATION;
    }
    
    ObDereferenceObject(TargetProcess);
    return Status;
}

// Write memory
NTSTATUS WriteKernelMemory(HANDLE Pid, ULONG_PTR Address, PVOID Buffer, SIZE_T Size, PSIZE_T BytesWritten) {
    PEPROCESS TargetProcess = NULL;
    NTSTATUS Status = STATUS_SUCCESS;
    SIZE_T ReturnSize = 0;
    
    if (!Buffer || !Size || !Address) {
        return STATUS_INVALID_PARAMETER;
    }
    
    Status = PsLookupProcessByProcessId(Pid, &TargetProcess);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    __try {
        Status = MmCopyVirtualMemory(PsGetCurrentProcess(), Buffer,
                                     TargetProcess, (PVOID)Address,
                                     Size, KernelMode, &ReturnSize);
        
        if (NT_SUCCESS(Status) && BytesWritten) {
            *BytesWritten = ReturnSize;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        Status = STATUS_ACCESS_VIOLATION;
    }
    
    ObDereferenceObject(TargetProcess);
    return Status;
}

// Get module base address
NTSTATUS GetModuleBase(HANDLE Pid, PCHAR ModuleName, PULONG_PTR BaseAddress, PSIZE_T ModuleSize) {
    PEPROCESS TargetProcess = NULL;
    NTSTATUS Status = STATUS_SUCCESS;
    KAPC_STATE ApcState = {0};
    PVOID Peb = NULL;
    PVOID Ldr = NULL;
    
    Status = PsLookupProcessByProcessId(Pid, &TargetProcess);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    
    // Attach to target process
    KeStackAttachProcess(TargetProcess, &ApcState);
    
    __try {
        // Read PEB
        Peb = PsGetProcessPeb(TargetProcess);
        if (Peb) {
            // Read PEB->Ldr
            // Simplified - real implementation would walk PEB_LDR_DATA
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        Status = STATUS_ACCESS_VIOLATION;
    }
    
    KeUnstackDetachProcess(&ApcState);
    ObDereferenceObject(TargetProcess);
    
    return Status;
}
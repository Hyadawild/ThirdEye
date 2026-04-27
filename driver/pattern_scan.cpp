#include "driver.h"

typedef struct _PATTERN {
    PUCHAR Pattern;
    PUCHAR Mask;
    SIZE_T Length;
} PATTERN, *PPATTERN;

// Search for pattern in kernel memory
ULONG_PTR FindPattern(PUCHAR Start, SIZE_T Size, PPATTERN Pattern) {
    for (SIZE_T i = 0; i < Size - Pattern->Length; i++) {
        BOOLEAN Found = TRUE;
        
        for (SIZE_T j = 0; j < Pattern->Length; j++) {
            if (Pattern->Mask[j] == 'x' && Start[i + j] != Pattern->Pattern[j]) {
                Found = FALSE;
                break;
            }
        }
        
        if (Found) {
            return (ULONG_PTR)(Start + i);
        }
    }
    
    return 0;
}

// Find ntoskrnl base address
ULONG_PTR GetNtoskrnlBase(PULONG_PTR Size) {
    ULONG_PTR Base = 0;
    ULONG_PTR ModuleSize = 0;
    
    // Scan loaded modules list
    PLDR_DATA_TABLE_ENTRY LdrEntry = (PLDR_DATA_TABLE_ENTRY)PsLoadedModuleList;
    PLIST_ENTRY CurrentEntry = PsLoadedModuleList.Flink;
    
    while (CurrentEntry != &PsLoadedModuleList) {
        LdrEntry = CONTAINING_RECORD(CurrentEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        
        if (LdrEntry->BaseDllName.Buffer && 
            _wcsicmp(LdrEntry->BaseDllName.Buffer, L"ntoskrnl.exe") == 0) {
            Base = (ULONG_PTR)LdrEntry->DllBase;
            ModuleSize = LdrEntry->SizeOfImage;
            break;
        }
        
        CurrentEntry = CurrentEntry->Flink;
    }
    
    if (Size) {
        *Size = ModuleSize;
    }
    
    return Base;
}

// Find specific function by signature
ULONG_PTR FindFunction(PCHAR FunctionName) {
    ULONG_PTR NtosBase = 0;
    ULONG_PTR NtosSize = 0;
    
    NtosBase = GetNtoskrnlBase(&NtosSize);
    if (!NtosBase) {
        return 0;
    }
    
    // Search for function in export table
    PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)NtosBase;
    PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)(NtosBase + DosHeader->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY ExportDir = (PIMAGE_EXPORT_DIRECTORY)(NtosBase + 
        NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    
    PDWORD Functions = (PDWORD)(NtosBase + ExportDir->AddressOfFunctions);
    PDWORD Names = (PDWORD)(NtosBase + ExportDir->AddressOfNames);
    PWORD Ordinals = (PWORD)(NtosBase + ExportDir->AddressOfNameOrdinals);
    
    for (DWORD i = 0; i < ExportDir->NumberOfNames; i++) {
        PCHAR CurrentName = (PCHAR)(NtosBase + Names[i]);
        
        if (strcmp(CurrentName, FunctionName) == 0) {
            return NtosBase + Functions[Ordinals[i]];
        }
    }
    
    return 0;
}
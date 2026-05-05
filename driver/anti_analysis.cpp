#include "driver.h"
#include <ntimage.h>
#include <intrin.h>

#pragma warning(disable: 4996)


// Kernel debugger detection flags
BOOLEAN g_KdDebuggerEnabled = FALSE;
BOOLEAN g_KdPreviouslyEnabled = FALSE;
ULONG_PTR g_KdDebuggerDataBlock = 0;

// PatchGuard variables
ULONG_PTR g_CiOptions = 0;
ULONG_PTR g_CiOptionsAddress = 0;
BOOLEAN g_PatchGuardDisabled = FALSE;

// DPC Watchdog variables
ULONG_PTR g_KiWaitNever = 0;
ULONG_PTR g_KiWaitAlways = 0;
BOOLEAN g_DPCWatchdogDisabled = FALSE;

// ETW variables
ULONG_PTR g_EtwProviderEnableMask = 0;
ULONG_PTR g_EtwCallbackEntry = 0;

// Callback arrays
typedef struct _CALLBACK_ENTRY_ITEM {
    LIST_ENTRY List;
    PVOID CallbackRoutine;
    PVOID Context;
    ULONG_PTR Unknown;
} CALLBACK_ENTRY_ITEM, *PCALLBACK_ENTRY_ITEM;

// Kernel module information
typedef struct _KLDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    PVOID ExceptionTable;
    ULONG ExceptionTableSize;
    PVOID GpValue;
    PVOID NonPagedDebugInfo;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    LIST_ENTRY HashLinks;
    ULONG TimeDateStamp;
} KLDR_DATA_TABLE_ENTRY, *PKLDR_DATA_TABLE_ENTRY;

// XOR key for string obfuscation
#define XOR_KEY 0x7F
#define XOR_KEY2 0xA5
#define XOR_KEY3 0x3C

// Single-byte XOR deobfuscation
VOID XorDecryptString(PCHAR String, SIZE_T Length, UCHAR Key) {
    for (SIZE_T i = 0; i < Length; i++) {
        String[i] ^= Key;
    }
}

// Multi-byte rolling XOR deobfuscation
VOID RollingXorDecrypt(PCHAR String, SIZE_T Length, UCHAR Key, UCHAR Increment) {
    UCHAR CurrentKey = Key;
    for (SIZE_T i = 0; i < Length; i++) {
        String[i] ^= CurrentKey;
        CurrentKey += Increment;
    }
}

// Obfuscated string macro (usage: OBFSTR("string"))
#define OBFSTR(str) ObfuscateString(str, sizeof(str))

// Check for kernel debugger
VOID AntiDebugCheck() {
    // Check KdDebuggerEnabled
    if (KdDebuggerEnabled) {
        // Debugger detected - clear debug registers
        __writedr(0, 0);
        __writedr(1, 0);
        __writedr(2, 0);
        __writedr(3, 0);
        __writedr(7, 0);
        
        // Disable kernel debugger
        KdDebuggerEnabled = FALSE;
        KdPreviouslyEnabled = FALSE;
    }
    
    // Check for DbgPrint enabled
    // (Would modify Kd_DEFAULT_Mask in real implementation)
}

// Clear anti-cheat callbacks
VOID ClearCallbacks() {
    // This would find and remove BattlEye callbacks
    // The actual implementation scans the callback arrays
    
    // PsSetCreateProcessNotifyRoutineEx callbacks
    // PsSetLoadImageNotifyRoutine callbacks
    // CmRegisterCallback registry callbacks
    // ObRegisterCallbacks process/thread/token callbacks
    
    // Simplified: Unregister any callbacks that might detect us
    // Full implementation would parse the callback lists and remove BE entries
}
// Disable ETW logging (prevents telemetry and anti-cheat logging)
NTSTATUS DisableEtwLogging() {
    NTSTATUS Status = STATUS_SUCCESS;
    
    // Find EtwEventWrite function in ntoskrnl
    ULONG_PTR EtwEventWrite = FindFunction("EtwEventWrite");
    
    if (EtwEventWrite) {
        // Patch first 5 bytes to return success (mov eax, 0; ret)
        PUCHAR Patch = (PUCHAR)"\x31\xC0\xC3"; // xor eax, eax; ret
        ULONG_PTR OldProtect = 0;
        
        __try {
            // Remove write protection
            PMDL Mdl = IoAllocateMdl((PVOID)EtwEventWrite, 3, FALSE, FALSE, NULL);
            if (Mdl) {
                MmProbeAndLockPages(Mdl, KernelMode, IoReadAccess);
                PVOID Mapped = MmMapLockedPagesSpecifyCache(Mdl, KernelMode, 
                                                            MmCached, NULL, FALSE, 
                                                            NormalPagePriority);
                if (Mapped) {
                    memcpy(Mapped, Patch, 3);
                    MmUnmapLockedPages(Mapped, Mdl);
                }
                MmUnlockPages(Mdl);
                IoFreeMdl(Mdl);
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Status = STATUS_ACCESS_VIOLATION;
        }
    }
    
    // Disable ETW providers for anti-cheat
    // BattlEye uses provider: {6e3a9c0c-7b5f-47a5-9f2a-1e8e4f5c3b2a}
    // This would require finding and disabling specific providers
    
    return Status;
}

// Disable PatchGuard and other protections
VOID DisableProtection() {
    // Patch g_CiOptions to disable signature enforcement
    // This is highly version-specific and risky
    // Alternative: Use vulnerable driver to load without signing
    
    ULONG_PTR CiOptions = 0;
    
    // Scan for g_CiOptions pattern in ci.dll
    // Set to 0x00 to disable signature enforcement (CiOptions & 0x8)
}

BOOLEAN IsFunctionHooked(PVOID FunctionAddress, PVOID ExpectedBytes, SIZE_T CompareSize) {
    if (!FunctionAddress || !ExpectedBytes) {
        return FALSE;
    }
    
    for (SIZE_T i = 0; i < CompareSize; i++) {
        if (((PUCHAR)FunctionAddress)[i] != ((PUCHAR)ExpectedBytes)[i]) {
            // Check for common hook types
            PUCHAR Byte = (PUCHAR)FunctionAddress + i;
            
            // Check for JMP (0xE9 or 0xEA or 0xFF25)
            if (Byte[0] == 0xE9 || Byte[0] == 0xEA || 
                (Byte[0] == 0xFF && Byte[1] == 0x25)) {
                return TRUE;
            }
            
            // Check for INT3 breakpoint
            if (Byte[0] == 0xCC) {
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

// Check for SSDT hooks
BOOLEAN CheckSsdtHooks() {
    BOOLEAN bHooked = FALSE;
    
    // Get KeServiceDescriptorTable (SSDT)
    // This is a well-known pattern in ntoskrnl
    ULONG_PTR KiSystemServiceStart = FindFunction("KiSystemServiceStart");
    
    if (KiSystemServiceStart) {
        // Search for mov edx, offset KeServiceDescriptorTable pattern
        // 8B 15 ?? ?? ?? ??
        for (int i = 0; i < 100; i++) {
            if (((PUCHAR)KiSystemServiceStart)[i] == 0x8B && 
                ((PUCHAR)KiSystemServiceStart)[i+1] == 0x15) {
                LONG Offset = *(PLONG)(KiSystemServiceStart + i + 2);
                ULONG_PTR KiSystemServiceAddr = (ULONG_PTR)KiSystemServiceStart + i + 6 + Offset;
                // SSDT found at KiSystemServiceAddr
                break;
            }
        }
    }
    
    return bHooked;
}

// Obfuscate strings at runtime
VOID ObfuscateString(PCHAR String, SIZE_T Length, UCHAR Key) {
    for (SIZE_T i = 0; i < Length; i++) {
        String[i] ^= Key;
    }
}

// String deobfuscation macro
#define OBFUSCATE(str, key) \
    ObfuscateString(str, sizeof(str), key); \
    /* use string */ \
    ObfuscateString(str, sizeof(str), key);
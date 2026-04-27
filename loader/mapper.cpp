#include <windows.h>
#include <iostream>
#include <vector>
#include <tlhelp32.h>

#pragma comment(lib, "advapi32.lib")

typedef HMODULE(WINAPI* pLoadLibraryA)(LPCSTR);
typedef FARPROC(WINAPI* pGetProcAddress)(HMODULE, LPCSTR);

typedef struct _MANUAL_MAPPING_DATA {
    pLoadLibraryA LoadLibraryA;
    pGetProcAddress GetProcAddress;
    HANDLE ProcessHandle;
    PVOID ImageBase;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_BASE_RELOCATION RelocData;
    PIMAGE_IMPORT_DESCRIPTOR ImportData;
} MANUAL_MAPPING_DATA, *PMANUAL_MAPPING_DATA;

class ManualMapper {
private:
    HANDLE m_hProcess;
    DWORD m_Pid;
    
    DWORD GetTargetProcessId(const char* ProcessName) {
        DWORD Pid = 0;
        HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        
        if (Snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);
            
            if (Process32First(Snapshot, &pe32)) {
                do {
                    if (_stricmp(pe32.szExeFile, ProcessName) == 0) {
                        Pid = pe32.th32ProcessID;
                        break;
                    }
                } while (Process32Next(Snapshot, &pe32));
            }
            
            CloseHandle(Snapshot);
        }
        
        return Pid;
    }
    
    PVOID AllocateMemoryInProcess(SIZE_T Size) {
        return VirtualAllocEx(m_hProcess, NULL, Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    
    BOOL WriteToProcess(PVOID Address, PVOID Buffer, SIZE_T Size) {
        SIZE_T BytesWritten = 0;
        return WriteProcessMemory(m_hProcess, Address, Buffer, Size, &BytesWritten);
    }
    
    PVOID ReadFromProcess(PVOID Address, SIZE_T Size) {
        PVOID Buffer = malloc(Size);
        SIZE_T BytesRead = 0;
        
        if (ReadProcessMemory(m_hProcess, Address, Buffer, Size, &BytesRead)) {
            return Buffer;
        }
        
        free(Buffer);
        return NULL;
    }
    
    VOID RelocateImage(PVOID ImageBase, ULONG_PTR OldBase, ULONG_PTR NewBase) {
        PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)ImageBase;
        PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)((PBYTE)ImageBase + DosHeader->e_lfanew);
        
        PIMAGE_BASE_RELOCATION RelocBlock = (PIMAGE_BASE_RELOCATION)((PBYTE)ImageBase + 
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
    
public:
    ManualMapper() : m_hProcess(NULL), m_Pid(0) {}
    
    ~ManualMapper() {
        if (m_hProcess) {
            CloseHandle(m_hProcess);
        }
    }
    
    BOOL MapToProcess(const char* TargetProcess, std::vector<BYTE>& DllImage) {
        // Get target process ID
        m_Pid = GetTargetProcessId(TargetProcess);
        if (!m_Pid) {
            printf("[!] Process not found: %s\n", TargetProcess);
            return FALSE;
        }
        
        printf("[*] Found process PID: %d\n", m_Pid);
        
        // Open process
        m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_Pid);
        if (!m_hProcess) {
            printf("[!] Failed to open process. Error: %d\n", GetLastError());
            return FALSE;
        }
        
        // Parse DLL image
        PIMAGE_DOS_HEADER DosHeader = (PIMAGE_DOS_HEADER)DllImage.data();
        PIMAGE_NT_HEADERS NtHeaders = (PIMAGE_NT_HEADERS)(DllImage.data() + DosHeader->e_lfanew);
        
        SIZE_T ImageSize = NtHeaders->OptionalHeader.SizeOfImage;
        ULONG_PTR ImageBase = NtHeaders->OptionalHeader.ImageBase;
        
        printf("[*] DLL image size: 0x%llX\n", ImageSize);
        
        // Allocate memory in target process
        PVOID RemoteBase = AllocateMemoryInProcess(ImageSize);
        if (!RemoteBase) {
            printf("[!] Failed to allocate memory in target. Error: %d\n", GetLastError());
            return FALSE;
        }
        
        printf("[+] Allocated at: 0x%p\n", RemoteBase);
        
        // Copy headers
        if (!WriteToProcess(RemoteBase, DllImage.data(), NtHeaders->OptionalHeader.SizeOfHeaders)) {
            printf("[!] Failed to write headers\n");
            return FALSE;
        }
        
        // Copy sections
        PIMAGE_SECTION_HEADER Section = IMAGE_FIRST_SECTION(NtHeaders);
        for (WORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {
            if (Section->SizeOfRawData) {
                if (!WriteToProcess((PBYTE)RemoteBase + Section->VirtualAddress,
                                   DllImage.data() + Section->PointerToRawData,
                                   Section->SizeOfRawData)) {
                    printf("[!] Failed to write section %d\n", i);
                    return FALSE;
                }
            }
            Section++;
        }
        
        // Read back image for relocation
        PVOID LocalCopy = ReadFromProcess(RemoteBase, ImageSize);
        if (!LocalCopy) {
            printf("[!] Failed to read back image\n");
            return FALSE;
        }
        
        // Perform relocation
        if ((ULONG_PTR)RemoteBase != ImageBase) {
            printf("[*] Relocating (0x%llX -> 0x%p)\n", ImageBase, RemoteBase);
            RelocateImage(LocalCopy, ImageBase, (ULONG_PTR)RemoteBase);
            
            // Write back relocated image
            Section = IMAGE_FIRST_SECTION(NtHeaders);
            for (WORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {
                if (Section->SizeOfRawData) {
                    WriteToProcess((PBYTE)RemoteBase + Section->VirtualAddress,
                                  (PBYTE)LocalCopy + Section->VirtualAddress,
                                  Section->SizeOfRawData);
                }
                Section++;
            }
        }
        
        // Get DLL entry point
        PIMAGE_EXPORT_DIRECTORY ExportDir = (PIMAGE_EXPORT_DIRECTORY)((PBYTE)LocalCopy + 
            NtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
        
        PDWORD Functions = (PDWORD)((PBYTE)LocalCopy + ExportDir->AddressOfFunctions);
        PDWORD Names = (PDWORD)((PBYTE)LocalCopy + ExportDir->AddressOfNames);
        PWORD Ordinals = (PWORD)((PBYTE)LocalCopy + ExportDir->AddressOfNameOrdinals);
        
        ULONG_PTR DllMain = 0;
        
        for (DWORD i = 0; i < ExportDir->NumberOfNames; i++) {
            PCHAR FunctionName = (PCHAR)((PBYTE)LocalCopy + Names[i]);
            
            if (strcmp(FunctionName, "DllMain") == 0) {
                DllMain = (ULONG_PTR)RemoteBase + Functions[Ordinals[i]];
                break;
            }
        }
        
        if (!DllMain) {
            printf("[!] DllMain not found\n");
            free(LocalCopy);
            return FALSE;
        }
        
        printf("[*] DllMain at: 0x%llX\n", DllMain);
        
        // Create remote thread to call DllMain
        HANDLE hThread = CreateRemoteThread(m_hProcess, NULL, 0,
                                            (LPTHREAD_START_ROUTINE)DllMain,
                                            RemoteBase, 0, NULL);
        
        if (!hThread) {
            printf("[!] Failed to create remote thread. Error: %d\n", GetLastError());
            free(LocalCopy);
            return FALSE;
        }
        
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        
        printf("[+] DLL injected successfully!\n");
        
        free(LocalCopy);
        return TRUE;
    }
};

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("   Manual Mapper - DLL Injector\n");
    printf("========================================\n\n");
    
    if (argc < 3) {
        printf("Usage: %s <process.exe> <inject.dll>\n", argv[0]);
        printf("Example: %s TslGame.exe SecureBridgeClient.dll\n", argv[0]);
        system("pause");
        return 1;
    }
    
    // Read DLL
    FILE* File = fopen(argv[2], "rb");
    if (!File) {
        printf("[!] Failed to open DLL: %s\n", argv[2]);
        system("pause");
        return 1;
    }
    
    fseek(File, 0, SEEK_END);
    long Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    
    std::vector<BYTE> DllImage(Size);
    fread(DllImage.data(), 1, Size, File);
    fclose(File);
    
    ManualMapper Mapper;
    
    if (Mapper.MapToProcess(argv[1], DllImage)) {
        printf("[+] Injection complete\n");
    } else {
        printf("[!] Injection failed\n");
    }
    
    system("pause");
    return 0;
}
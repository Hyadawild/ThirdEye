#include <windows.h>

// Reflective loader function
__declspec(dllexport) DWORD WINAPI ReflectiveLoader(LPVOID lpParameter) {
    // Get current image base
    DWORD dwImageBase = (DWORD)GetModuleHandle(NULL);
    
    // Find DLL image in memory
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)dwImageBase;
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(dwImageBase + pDosHeader->e_lfanew);
    
    // Load dependencies
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)(dwImageBase + 
        pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    
    while (pImportDesc->Name) {
        char* szModule = (char*)(dwImageBase + pImportDesc->Name);
        HMODULE hModule = LoadLibraryA(szModule);
        
        if (!hModule) {
            return FALSE;
        }
        
        PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)(dwImageBase + pImportDesc->OriginalFirstThunk);
        PIMAGE_THUNK_DATA pFuncThunk = (PIMAGE_THUNK_DATA)(dwImageBase + pImportDesc->FirstThunk);
        
        while (pThunk->u1.AddressOfData) {
            FARPROC pFunc;
            
            if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                pFunc = GetProcAddress(hModule, (LPCSTR)(pThunk->u1.Ordinal & 0xFFFF));
            } else {
                PIMAGE_IMPORT_BY_NAME pImportByName = (PIMAGE_IMPORT_BY_NAME)(dwImageBase + pThunk->u1.AddressOfData);
                pFunc = GetProcAddress(hModule, pImportByName->Name);
            }
            
            if (!pFunc) {
                return FALSE;
            }
            
            pFuncThunk->u1.Function = (ULONG_PTR)pFunc;
            pThunk++;
            pFuncThunk++;
        }
        
        pImportDesc++;
    }
    
    // Process relocations
    DWORD dwDelta = dwImageBase - pNtHeaders->OptionalHeader.ImageBase;
    
    if (dwDelta) {
        PIMAGE_BASE_RELOCATION pRelocBlock = (PIMAGE_BASE_RELOCATION)(dwImageBase + 
            pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        
        while (pRelocBlock->VirtualAddress) {
            DWORD dwNumEntries = (pRelocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            PWORD pRelocEntries = (PWORD)((PBYTE)pRelocBlock + sizeof(IMAGE_BASE_RELOCATION));
            
            for (DWORD i = 0; i < dwNumEntries; i++) {
                WORD wType = pRelocEntries[i] >> 12;
                WORD wOffset = pRelocEntries[i] & 0xFFF;
                
                if (wType == IMAGE_REL_BASED_HIGHLOW) {
                    DWORD* pFixup = (DWORD*)((PBYTE)pRelocBlock + pRelocBlock->VirtualAddress + wOffset);
                    *pFixup += dwDelta;
                } else if (wType == IMAGE_REL_BASED_DIR64) {
                    ULONGLONG* pFixup = (ULONGLONG*)((PBYTE)pRelocBlock + pRelocBlock->VirtualAddress + wOffset);
                    *pFixup += dwDelta;
                }
            }
            
            pRelocBlock = (PIMAGE_BASE_RELOCATION)((PBYTE)pRelocBlock + pRelocBlock->SizeOfBlock);
        }
    }
    
    // Call DllMain
    DWORD dwDllMain = (DWORD)(dwImageBase + pNtHeaders->OptionalHeader.AddressOfEntryPoint);
    
    if (dwDllMain) {
        BOOL (WINAPI* DllMain)(HINSTANCE, DWORD, LPVOID) = (BOOL (WINAPI*)(HINSTANCE, DWORD, LPVOID))dwDllMain;
        DllMain((HINSTANCE)dwImageBase, DLL_PROCESS_ATTACH, lpParameter);
    }
    
    return TRUE;
}
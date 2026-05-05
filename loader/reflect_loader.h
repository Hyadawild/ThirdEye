#pragma once
#include <windows.h>
#include <stdio.h>
#define REFLECTIVE_CODE __declspec(dllexport) DWORD WINAPI ReflectiveLoader(LPVOID lpParameter)

typedef DWORD (WINAPI *REFLECTIVE_LOADER)(LPVOID);

// Function to prepare reflective loader stub
DWORD WINAPI ReflectiveLoaderStub(LPVOID lpParameter);

// Load reflective loader DLL first, then use it to load other DLLs

int main() {
    // Read your target DLL into memory
    FILE* f = fopen("SecureBridgeClient.dll", "rb");
    fseek(f, 0, SEEK_END);
    SIZE_T dllSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    LPVOID lpDllData = VirtualAlloc(NULL, dllSize, MEM_COMMIT, PAGE_READWRITE);
    fread(lpDllData, 1, dllSize, f);
    fclose(f);
    
    // Get reflective loader function pointer
    HMODULE hReflect = LoadLibraryA("reflective_loader.dll");
    REFLECTIVE_LOADER pReflectiveLoad = (REFLECTIVE_LOADER)GetProcAddress(hReflect, "ReflectiveLoad");
    
    // Load the DLL reflectively
    LPVOID lpParam = NULL; // Optional parameter for DllMain
    pReflectiveLoad(lpDllData, dllSize, lpParam);
    
    VirtualFree(lpDllData, 0, MEM_RELEASE);
    
    return 0;
}
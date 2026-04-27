#pragma once

#define REFLECTIVE_CODE __declspec(dllexport) DWORD WINAPI ReflectiveLoader(LPVOID lpParameter)

typedef DWORD (WINAPI *REFLECTIVE_LOADER)(LPVOID);

// Function to prepare reflective loader stub
DWORD WINAPI ReflectiveLoaderStub(LPVOID lpParameter);
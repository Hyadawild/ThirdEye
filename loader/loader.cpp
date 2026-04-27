#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ntdll.lib")

typedef NTSTATUS(NTAPI* pNtLoadDriver)(PUNICODE_STRING DriverServiceName);
typedef NTSTATUS(NTAPI* pNtUnloadDriver)(PUNICODE_STRING DriverServiceName);
typedef NTSTATUS(NTAPI* pRtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);

class DriverLoader {
private:
    pNtLoadDriver NtLoadDriver;
    pNtUnloadDriver NtUnloadDriver;
    pRtlAdjustPrivilege RtlAdjustPrivilege;
    
    BOOL EnablePrivilege() {
        BOOLEAN Enabled = FALSE;
        RtlAdjustPrivilege = (pRtlAdjustPrivilege)GetProcAddress(GetModuleHandle("ntdll.dll"), "RtlAdjustPrivilege");
        
        if (RtlAdjustPrivilege) {
            // SE_LOAD_DRIVER_PRIVILEGE = 10
            return RtlAdjustPrivilege(10, TRUE, FALSE, &Enabled) == 0;
        }
        return FALSE;
    }
    
public:
    DriverLoader() {
        NtLoadDriver = (pNtLoadDriver)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtLoadDriver");
        NtUnloadDriver = (pNtUnloadDriver)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtUnloadDriver");
    }
    
    BOOL LoadDriver(std::wstring DriverPath, std::wstring ServiceName) {
        if (!EnablePrivilege()) {
            std::cout << "[!] Failed to enable load driver privilege" << std::endl;
            return FALSE;
        }
        
        // Create registry key
        std::wstring RegistryPath = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + ServiceName;
        
        HKEY hKey;
        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, 
            (L"SYSTEM\\CurrentControlSet\\Services\\" + ServiceName).c_str(),
            0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            
            // Set driver image path
            RegSetValueEx(hKey, L"ImagePath", 0, REG_EXPAND_SZ, (BYTE*)DriverPath.c_str(), 
                         (DWORD)((DriverPath.length() + 1) * sizeof(wchar_t)));
            
            // Set type - kernel driver
            DWORD Type = 1;
            RegSetValueEx(hKey, L"Type", 0, REG_DWORD, (BYTE*)&Type, sizeof(DWORD));
            
            // Set start type - demand start
            DWORD Start = 3;
            RegSetValueEx(hKey, L"Start", 0, REG_DWORD, (BYTE*)&Start, sizeof(DWORD));
            
            // Set error control - normal
            DWORD ErrorControl = 1;
            RegSetValueEx(hKey, L"ErrorControl", 0, REG_DWORD, (BYTE*)&ErrorControl, sizeof(DWORD));
            
            RegCloseKey(hKey);
        }
        
        // Load driver
        UNICODE_STRING ServiceNameUnicode;
        RtlInitUnicodeString(&ServiceNameUnicode, ServiceName.c_str());
        
        NTSTATUS Status = NtLoadDriver(&ServiceNameUnicode);
        
        if (Status == 0) {
            std::cout << "[+] Driver loaded successfully" << std::endl;
            return TRUE;
        } else {
            std::cout << "[!] Failed to load driver: 0x" << std::hex << Status << std::endl;
            return FALSE;
        }
    }
    
    BOOL UnloadDriver(std::wstring ServiceName) {
        UNICODE_STRING ServiceNameUnicode;
        RtlInitUnicodeString(&ServiceNameUnicode, ServiceName.c_str());
        
        NTSTATUS Status = NtUnloadDriver(&ServiceNameUnicode);
        
        // Delete registry key
        RegDeleteKey(HKEY_LOCAL_MACHINE, (L"SYSTEM\\CurrentControlSet\\Services\\" + ServiceName).c_str());
        
        return Status == 0;
    }
};

int main() {
    DriverLoader loader;
    
    std::cout << "=== SecureBridge Driver Loader ===" << std::endl;
    std::cout << "[1] Load Driver" << std::endl;
    std::cout << "[2] Unload Driver" << std::endl;
    std::cout << "[3] Exit" << std::endl;
    std::cout << "Choice: ";
    
    int choice;
    std::cin >> choice;
    
    switch (choice) {
        case 1:
            loader.LoadDriver(L"C:\\Windows\\System32\\drivers\\SecureBridge.sys", L"SecureBridge");
            break;
        case 2:
            loader.UnloadDriver(L"SecureBridge");
            break;
        case 3:
            return 0;
    }
    
    system("pause");
    return 0;
}
#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>

void PrintBanner() {
    std::cout << R"(
 _____ _     ____  _  ____  _     ____  ____  ____  _____ ____ 
/__ __Y \ /|/  __\/ \/  _ \/ \   /  _ \/  _ \/  _ \/  __//  __\
  / \ | |_|||  \/|| || | \|| |   | / \|| / \|| | \||  \  |  \/|
  | | | | |||    /| || |_/|| |_/\| \_/|| |-||| |_/||  /_ |    /
  \_/ \_/ \|\_/\_\\_/\____/\____/\____/\_/ \|\____/\____\\_/\_\
                                                               
            
     SecureBridge Kernel Driver Loader
)" << std::endl;
}

int main() {
    PrintBanner();
    
    SetConsoleTitle(L"SecureBridge Loader");
    system("color 0A");
    
    // Enable required privileges
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        TOKEN_PRIVILEGES tp;
        LUID luid;
        
        LookupPrivilegeValue(NULL, SE_LOAD_DRIVER_NAME, &luid);
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        CloseHandle(hToken);
    }
    
    std::cout << "[*] Checking for existing driver..." << std::endl;
    
    // Check if driver is already loaded
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE hService = OpenServiceW(hSCManager, L"SecureBridge", SERVICE_QUERY_STATUS);
    
    if (hService) {
        SERVICE_STATUS status;
        if (QueryServiceStatus(hService, &status)) {
            if (status.dwCurrentState == SERVICE_RUNNING) {
                std::cout << "[!] Driver is already loaded!" << std::endl;
                std::cout << "[*] Unload first? (y/n): ";
                char choice;
                std::cin >> choice;
                if (choice == 'y' || choice == 'Y') {
                    system("SecureBridge_unload.bat");
                } else {
                    CloseServiceHandle(hService);
                    CloseServiceHandle(hSCManager);
                    return 0;
                }
            }
        }
        CloseServiceHandle(hService);
    }
    CloseServiceHandle(hSCManager);
    
    std::cout << "\n=== Loading Options ===" << std::endl;
    std::cout << "[1] Load via Service (Requires Signature)" << std::endl;
    std::cout << "[2] Load via KDMapper (Manual Map - No Trace)" << std::endl;
    std::cout << "[3] Load via Vulnerable Driver" << std::endl;
    std::cout << "[4] Exit" << std::endl;
    std::cout << "Choice: ";
    
    int choice;
    std::cin >> choice;
    
    switch (choice) {
        case 1:
            std::cout << "\n[*] Loading via service..." << std::endl;
            system("load_driver_standard.bat");
            break;
        case 2:
            std::cout << "\n[*] Loading via KDMapper..." << std::endl;
            
            // Check if driver exists
            std::ifstream DriverFile("SecureBridge.sys");
            if (!DriverFile.good()) {
                std::cout << "[!] SecureBridge.sys not found in current directory!" << std::endl;
                system("pause");
                return 1;
            }
            DriverFile.close();
            
            system("kdmapper.exe SecureBridge.sys");
            break;
        case 3:
            std::cout << "\n[*] Loading via vulnerable driver..." << std::endl;
            system("load_vulnerable.bat");
            break;
        case 4:
            return 0;
        default:
            std::cout << "[!] Invalid choice!" << std::endl;
            system("pause");
            return 1;
    }
    
    // Verify driver loaded
    std::cout << "\n[*] Verifying driver status..." << std::endl;
    
    HANDLE hDevice = CreateFileW(L"\\\\.\\SecureBridge", 
                                 GENERIC_READ | GENERIC_WRITE,
                                 0, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice != INVALID_HANDLE_VALUE) {
        std::cout << "[+] Driver loaded successfully!" << std::endl;
        CloseHandle(hDevice);
        
        // Optional: Test communication
        std::cout << "[*] Would you like to test driver communication? (y/n): ";
        char test;
        std::cin >> test;
        if (test == 'y' || test == 'Y') {
            system("test_client.exe");
        }
    } else {
        std::cout << "[!] Driver not found! Load may have failed." << std::endl;
        std::cout << "[*] Check Windows test signing is enabled." << std::endl;
        std::cout << "[*] Run as Administrator." << std::endl;
    }
    
    std::cout << "\nPress any key to exit...";
    system("pause >nul");
    
    return 0;
}
#include <windows.h>
#include <iostream>

#define IOCTL_READ_MEMORY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main() {
    HANDLE hDevice = CreateFile(L"\\\\.\\SecureBridge", 
                                GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cout << "[!] Failed to connect to driver. Error: " << GetLastError() << std::endl;
        system("pause");
        return 1;
    }
    
    std::cout << "[+] Connected to driver!" << std::endl;
    
    // Test memory read
    int test_value = 0x12345678;
    int read_value = 0;
    
    DWORD returned;
    BOOL result = DeviceIoControl(hDevice, IOCTL_READ_MEMORY,
                                  &test_value, sizeof(test_value),
                                  &read_value, sizeof(read_value),
                                  &returned, NULL);
    
    if (result) {
        std::cout << "[+] IOCTL test passed!" << std::endl;
        std::cout << "[*] Read value: 0x" << std::hex << read_value << std::endl;
    } else {
        std::cout << "[!] IOCTL test failed. Error: " << GetLastError() << std::endl;
    }
    
    CloseHandle(hDevice);
    
    system("pause");
    return 0;
}
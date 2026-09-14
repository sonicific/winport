#pragma once

#include <windows.h>
#include <bluetoothapis.h>

#include <string>
#include <vector>

namespace sonic79 {

struct BluetoothDeviceRecord {
    BLUETOOTH_ADDRESS address{};
    std::wstring name;
    ULONG class_of_device = 0;
    SYSTEMTIME last_seen{};
    SYSTEMTIME last_used{};
    bool authenticated = false;
    bool remembered = false;
};

struct BluetoothEnumerationResult {
    std::vector<BluetoothDeviceRecord> devices;
    DWORD error = ERROR_SUCCESS;
};

class BluetoothManager {
public:
    static BluetoothEnumerationResult EnumerateDisconnected();
    static DWORD Remove(const BLUETOOTH_ADDRESS& address);
    static std::wstring FormatAddress(const BLUETOOTH_ADDRESS& address);
    static std::wstring FormatClass(ULONG class_of_device);
};

}  // namespace sonic79

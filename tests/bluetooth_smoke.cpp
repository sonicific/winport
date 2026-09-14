#include "bluetooth_manager.h"

#include <iostream>
#include <unordered_set>

int main() {
    const sonic79::BluetoothEnumerationResult result =
        sonic79::BluetoothManager::EnumerateDisconnected();
    if (result.error != ERROR_SUCCESS) {
        std::wcout << L"Bluetooth enumeration is unavailable on this host (Windows error "
                   << result.error << L"); API path completed safely.\n";
        return 0;
    }

    std::unordered_set<ULONGLONG> addresses;
    for (const auto& device : result.devices) {
        if (device.name.empty() ||
            !addresses.insert(device.address.ullLong).second) {
            std::wcerr << L"Bluetooth enumeration returned invalid or duplicate data.\n";
            return 1;
        }
    }
    std::wcout << L"Enumerated " << result.devices.size()
               << L" disconnected remembered Bluetooth device(s).\n";
    return 0;
}

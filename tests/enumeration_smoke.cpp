#include "device_manager.h"

#include <iostream>

int main() {
    const sonic79::EnumerationResult result =
        sonic79::DeviceManager::EnumerateNonPresent({});
    if (result.win32_error != ERROR_SUCCESS) {
        std::wcerr << L"Enumeration failed with Windows error "
                   << result.win32_error << L'\n';
        return 1;
    }

    std::wcout << L"Enumerated " << result.devices.size()
               << L" non-present, non-protected device(s).\n";
    return 0;
}

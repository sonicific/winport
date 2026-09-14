#include "bluetooth_manager.h"

#include <array>
#include <unordered_set>

namespace sonic79 {

BluetoothEnumerationResult BluetoothManager::EnumerateDisconnected() {
    BluetoothEnumerationResult result;
    BLUETOOTH_FIND_RADIO_PARAMS radio_parameters{sizeof(radio_parameters)};
    HANDLE radio = nullptr;
    HBLUETOOTH_RADIO_FIND radio_find =
        BluetoothFindFirstRadio(&radio_parameters, &radio);
    if (radio_find == nullptr) {
        const DWORD error = GetLastError();
        if (error != ERROR_NO_MORE_ITEMS && error != ERROR_SUCCESS) {
            result.error = error;
        }
        return result;
    }

    std::unordered_set<ULONGLONG> seen;
    do {
        BLUETOOTH_DEVICE_SEARCH_PARAMS search{};
        search.dwSize = sizeof(search);
        search.fReturnAuthenticated = TRUE;
        search.fReturnRemembered = TRUE;
        search.fReturnUnknown = FALSE;
        search.fReturnConnected = TRUE;
        search.fIssueInquiry = FALSE;
        search.cTimeoutMultiplier = 1;
        search.hRadio = radio;

        BLUETOOTH_DEVICE_INFO info{sizeof(info)};
        HBLUETOOTH_DEVICE_FIND device_find =
            BluetoothFindFirstDevice(&search, &info);
        if (device_find != nullptr) {
            do {
                if (!info.fConnected && seen.insert(info.Address.ullLong).second) {
                    BluetoothDeviceRecord device;
                    device.address = info.Address;
                    device.name = info.szName[0] != L'\0' ? info.szName : L"(unnamed)";
                    device.class_of_device = info.ulClassofDevice;
                    device.last_seen = info.stLastSeen;
                    device.last_used = info.stLastUsed;
                    device.authenticated = info.fAuthenticated != FALSE;
                    device.remembered = info.fRemembered != FALSE;
                    result.devices.push_back(std::move(device));
                }
                info = BLUETOOTH_DEVICE_INFO{sizeof(info)};
            } while (BluetoothFindNextDevice(device_find, &info));
            const DWORD find_error = GetLastError();
            if (find_error != ERROR_NO_MORE_ITEMS && find_error != ERROR_SUCCESS &&
                result.error == ERROR_SUCCESS) {
                result.error = find_error;
            }
            BluetoothFindDeviceClose(device_find);
        } else {
            const DWORD find_error = GetLastError();
            if (find_error != ERROR_NO_MORE_ITEMS && find_error != ERROR_SUCCESS &&
                result.error == ERROR_SUCCESS) {
                result.error = find_error;
            }
        }

        CloseHandle(radio);
        radio = nullptr;
    } while (BluetoothFindNextRadio(radio_find, &radio));

    const DWORD radio_error = GetLastError();
    if (radio_error != ERROR_NO_MORE_ITEMS && radio_error != ERROR_SUCCESS &&
        result.error == ERROR_SUCCESS) {
        result.error = radio_error;
    }

    BluetoothFindRadioClose(radio_find);
    return result;
}

DWORD BluetoothManager::Remove(const BLUETOOTH_ADDRESS& address) {
    BLUETOOTH_ADDRESS mutable_address = address;
    return BluetoothRemoveDevice(&mutable_address);
}

std::wstring BluetoothManager::FormatAddress(const BLUETOOTH_ADDRESS& address) {
    std::array<wchar_t, 18> buffer{};
    swprintf_s(buffer.data(), buffer.size(), L"%02X:%02X:%02X:%02X:%02X:%02X",
               address.rgBytes[5], address.rgBytes[4], address.rgBytes[3],
               address.rgBytes[2], address.rgBytes[1], address.rgBytes[0]);
    return buffer.data();
}

std::wstring BluetoothManager::FormatClass(ULONG class_of_device) {
    const ULONG major = (class_of_device >> 8) & 0x1F;
    const wchar_t* name = L"Unknown";
    switch (major) {
        case 0x01:
            name = L"Computer";
            break;
        case 0x02:
            name = L"Phone";
            break;
        case 0x03:
            name = L"Network";
            break;
        case 0x04:
            name = L"Audio/Video";
            break;
        case 0x05:
            name = L"Peripheral";
            break;
        case 0x06:
            name = L"Imaging";
            break;
        case 0x07:
            name = L"Wearable";
            break;
        case 0x08:
            name = L"Toy";
            break;
        case 0x09:
            name = L"Health";
            break;
        default:
            break;
    }

    std::array<wchar_t, 64> buffer{};
    swprintf_s(buffer.data(), buffer.size(), L"%s (0x%08X)", name,
               class_of_device);
    return buffer.data();
}

}  // namespace sonic79

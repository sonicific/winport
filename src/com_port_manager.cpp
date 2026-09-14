#include "com_port_manager.h"

#include "device_rules.h"

#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>

#include <algorithm>
#include <string>
#include <vector>

namespace sonic79 {
namespace {

constexpr wchar_t kEnumPath[] = L"SYSTEM\\CurrentControlSet\\Enum\\";
constexpr wchar_t kComArbiterPath[] =
    L"SYSTEM\\CurrentControlSet\\Control\\COM Name Arbiter";

class UniqueRegKey {
public:
    ~UniqueRegKey() {
        if (key_ != nullptr) {
            RegCloseKey(key_);
        }
    }
    HKEY* put() { return &key_; }
    HKEY get() const { return key_; }

private:
    HKEY key_ = nullptr;
};

std::wstring ReadPortName(std::wstring_view instance_id) {
    std::wstring key_path = kEnumPath;
    key_path.append(instance_id);
    key_path.append(L"\\Device Parameters");

    UniqueRegKey key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path.c_str(), 0,
                      KEY_QUERY_VALUE | KEY_WOW64_64KEY, key.put()) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD byte_count = 0;
    if (RegQueryValueExW(key.get(), L"PortName", nullptr, &type, nullptr,
                         &byte_count) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || byte_count < sizeof(wchar_t)) {
        return {};
    }

    std::vector<wchar_t> buffer(byte_count / sizeof(wchar_t) + 1);
    if (RegQueryValueExW(key.get(), L"PortName", nullptr, &type,
                         reinterpret_cast<BYTE*>(buffer.data()),
                         &byte_count) != ERROR_SUCCESS) {
        return {};
    }
    return buffer.data();
}

bool EqualsInsensitive(std::wstring_view left, std::wstring_view right) {
    return left.size() == right.size() &&
           CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool AnotherDeviceUsesPort(std::wstring_view port_name,
                           std::wstring_view removed_instance_id) {
    HDEVINFO raw_set =
        SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (raw_set == INVALID_HANDLE_VALUE) {
        return true;
    }

    bool used = false;
    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    for (DWORD index = 0;; ++index) {
        if (!SetupDiEnumDeviceInfo(raw_set, index, &info)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) {
                used = true;  // Fail closed: never clear ComDB after a partial scan.
            }
            break;
        }
        ULONG id_length = 0;
        if (CM_Get_Device_ID_Size(&id_length, info.DevInst, 0) != CR_SUCCESS) {
            continue;
        }
        std::vector<wchar_t> id(id_length + 1);
        if (CM_Get_Device_IDW(info.DevInst, id.data(), static_cast<ULONG>(id.size()), 0) !=
            CR_SUCCESS) {
            continue;
        }
        if (EqualsInsensitive(id.data(), removed_instance_id)) {
            continue;
        }
        if (EqualsInsensitive(ReadPortName(id.data()), port_name)) {
            used = true;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(raw_set);
    return used;
}

}  // namespace

bool ComPortManager::ReleaseReservationIfUnused(
    std::wstring_view port_name, std::wstring_view removed_instance_id) {
    const auto port_number = ParseComPortNumber(port_name);
    if (!port_number || AnotherDeviceUsesPort(port_name, removed_instance_id)) {
        return false;
    }

    UniqueRegKey key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kComArbiterPath, 0,
                      KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                      key.put()) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD byte_count = 0;
    if (RegQueryValueExW(key.get(), L"ComDB", nullptr, &type, nullptr, &byte_count) !=
            ERROR_SUCCESS ||
        type != REG_BINARY) {
        return false;
    }

    const size_t bit_index = static_cast<size_t>(*port_number - 1);
    const size_t byte_index = bit_index / 8;
    if (byte_index >= byte_count) {
        return false;
    }

    std::vector<BYTE> com_database(byte_count);
    if (RegQueryValueExW(key.get(), L"ComDB", nullptr, &type, com_database.data(),
                         &byte_count) != ERROR_SUCCESS) {
        return false;
    }

    const BYTE mask = static_cast<BYTE>(1u << (bit_index % 8));
    if ((com_database[byte_index] & mask) == 0) {
        return false;
    }
    com_database[byte_index] &= static_cast<BYTE>(~mask);

    return RegSetValueExW(key.get(), L"ComDB", 0, REG_BINARY, com_database.data(),
                          byte_count) == ERROR_SUCCESS;
}

}  // namespace sonic79

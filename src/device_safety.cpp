#include "device_safety.h"

#include "device_rules.h"

#include <windows.h>

#include <string>
#include <vector>

namespace sonic79 {
namespace {

constexpr wchar_t kEnumPath[] = L"SYSTEM\\CurrentControlSet\\Enum\\";
constexpr wchar_t kAffinitySuffix[] =
    L"\\Device Parameters\\Interrupt Management\\Affinity Policy";

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

bool HasNonZeroRegistryValue(HKEY key, const wchar_t* value_name) {
    DWORD type = REG_NONE;
    DWORD byte_count = 0;
    if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &byte_count) !=
            ERROR_SUCCESS ||
        byte_count == 0 ||
        (type != REG_BINARY && type != REG_DWORD && type != REG_QWORD)) {
        return false;
    }

    std::vector<BYTE> data(byte_count);
    if (RegQueryValueExW(key, value_name, nullptr, &type, data.data(), &byte_count) !=
        ERROR_SUCCESS) {
        return false;
    }
    for (const BYTE byte : data) {
        if (byte != 0) {
            return true;
        }
    }
    return false;
}

bool HasExplicitInterruptAffinity(std::wstring_view instance_id) {
    std::wstring path = kEnumPath;
    path.append(instance_id);
    path.append(kAffinitySuffix);

    UniqueRegKey key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0,
                      KEY_QUERY_VALUE | KEY_WOW64_64KEY,
                      key.put()) != ERROR_SUCCESS) {
        return false;
    }

    DWORD policy = 0;
    DWORD type = REG_NONE;
    DWORD size = sizeof(policy);
    const bool explicit_policy =
        RegQueryValueExW(key.get(), L"DevicePolicy", nullptr, &type,
                         reinterpret_cast<BYTE*>(&policy), &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(policy) && policy >= 3;

    return explicit_policy ||
           HasNonZeroRegistryValue(key.get(), L"AssignmentSetOverride");
}

}  // namespace

DeviceProtection AnalyzeDeviceProtection(std::wstring_view instance_id) {
    DeviceProtection protection = DeviceProtection::none;
    switch (ClassifyProtectedDevice(instance_id)) {
        case ProtectedDeviceKind::root:
            protection |= DeviceProtection::root_enumerated;
            break;
        case ProtectedDeviceKind::swd:
        case ProtectedDeviceKind::sw:
            protection |= DeviceProtection::software_device;
            break;
        case ProtectedDeviceKind::none:
            break;
    }
    if (HasExplicitInterruptAffinity(instance_id)) {
        protection |= DeviceProtection::interrupt_affinity;
    }
    return protection;
}

std::wstring DescribeDeviceProtection(DeviceProtection protection) {
    if (!IsProtected(protection)) {
        return L"Ghost";
    }

    std::wstring result;
    const auto append = [&result](std::wstring_view text) {
        if (!result.empty()) {
            result += L"; ";
        }
        result.append(text);
    };
    if (HasProtection(protection, DeviceProtection::interrupt_affinity)) {
        append(L"IRQ affinity configured");
    }
    if (HasProtection(protection, DeviceProtection::root_enumerated)) {
        append(L"ROOT device");
    }
    if (HasProtection(protection, DeviceProtection::software_device)) {
        append(L"Software device");
    }
    return result;
}

}  // namespace sonic79

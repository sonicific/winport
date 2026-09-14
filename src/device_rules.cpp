#include "device_rules.h"

#include <cwctype>
#include <limits>

namespace sonic79 {
namespace {

bool StartsWithInsensitive(std::wstring_view value, std::wstring_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }

    for (size_t index = 0; index < prefix.size(); ++index) {
        if (std::towupper(value[index]) != std::towupper(prefix[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace

ProtectedDeviceKind ClassifyProtectedDevice(std::wstring_view instance_id) {
    if (StartsWithInsensitive(instance_id, L"HTREE\\ROOT\\") ||
        StartsWithInsensitive(instance_id, L"ROOT\\")) {
        return ProtectedDeviceKind::root;
    }
    if (StartsWithInsensitive(instance_id, L"SWD\\")) {
        return ProtectedDeviceKind::swd;
    }
    if (StartsWithInsensitive(instance_id, L"SW\\{")) {
        return ProtectedDeviceKind::sw;
    }
    return ProtectedDeviceKind::none;
}

bool ShouldIncludeDevice(std::wstring_view instance_id, const DeviceFilters& filters) {
    switch (ClassifyProtectedDevice(instance_id)) {
        case ProtectedDeviceKind::root:
            return filters.include_root;
        case ProtectedDeviceKind::swd:
            return filters.include_swd;
        case ProtectedDeviceKind::sw:
            return filters.include_sw;
        case ProtectedDeviceKind::none:
            return true;
    }
    return true;
}

std::optional<unsigned int> ParseComPortNumber(std::wstring_view port_name) {
    if (!StartsWithInsensitive(port_name, L"COM") || port_name.size() <= 3) {
        return std::nullopt;
    }

    unsigned long long value = 0;
    for (size_t index = 3; index < port_name.size(); ++index) {
        const wchar_t character = port_name[index];
        if (character < L'0' || character > L'9') {
            return std::nullopt;
        }
        value = value * 10 + static_cast<unsigned int>(character - L'0');
        if (value > std::numeric_limits<unsigned int>::max()) {
            return std::nullopt;
        }
    }

    if (value == 0) {
        return std::nullopt;
    }
    return static_cast<unsigned int>(value);
}

}  // namespace sonic79

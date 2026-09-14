#pragma once

#include <optional>
#include <string_view>

namespace sonic79 {

struct DeviceFilters {
    bool include_root = false;
    bool include_swd = false;
    bool include_sw = false;
};

enum class ProtectedDeviceKind {
    none,
    root,
    swd,
    sw,
};

ProtectedDeviceKind ClassifyProtectedDevice(std::wstring_view instance_id);
bool ShouldIncludeDevice(std::wstring_view instance_id, const DeviceFilters& filters);
std::optional<unsigned int> ParseComPortNumber(std::wstring_view port_name);

}  // namespace sonic79

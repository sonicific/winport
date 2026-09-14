#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sonic79 {

enum class DeviceProtection : std::uint32_t {
    none = 0,
    root_enumerated = 1u << 0,
    software_device = 1u << 1,
    interrupt_affinity = 1u << 2,
};

constexpr DeviceProtection operator|(DeviceProtection left, DeviceProtection right) {
    return static_cast<DeviceProtection>(static_cast<std::uint32_t>(left) |
                                         static_cast<std::uint32_t>(right));
}

constexpr DeviceProtection& operator|=(DeviceProtection& left,
                                       DeviceProtection right) {
    left = left | right;
    return left;
}

constexpr bool HasProtection(DeviceProtection value, DeviceProtection flag) {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0;
}

constexpr bool IsProtected(DeviceProtection value) {
    return value != DeviceProtection::none;
}

DeviceProtection AnalyzeDeviceProtection(std::wstring_view instance_id);
std::wstring DescribeDeviceProtection(DeviceProtection protection);

}  // namespace sonic79

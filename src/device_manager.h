#pragma once

#include "device_rules.h"

#include <windows.h>
#include <cfgmgr32.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sonic79 {

struct DeviceRecord {
    std::wstring instance_id;
    std::wstring name;
    std::wstring class_name;
    std::wstring enumerator;
    std::wstring service;
    std::wstring com_port;
    std::optional<FILETIME> last_connected;
    DWORD status = 0;
    DWORD problem_code = 0;
};

struct EnumerationResult {
    std::vector<DeviceRecord> devices;
    DWORD win32_error = ERROR_SUCCESS;
};

struct RemovalResult {
    bool removed = false;
    bool still_present = false;
    bool reboot_required = false;
    CONFIGRET config_result = CR_SUCCESS;
    DWORD win32_error = ERROR_SUCCESS;
    bool com_reservation_released = false;
};

using EnumerationProgress = std::function<void(size_t scanned, size_t found)>;

class DeviceManager {
public:
    static EnumerationResult EnumerateNonPresent(
        const DeviceFilters& filters, const EnumerationProgress& progress = {});
    static RemovalResult Remove(const DeviceRecord& device);
    static bool OpenProperties(HWND owner, const DeviceRecord& device);
};

}  // namespace sonic79

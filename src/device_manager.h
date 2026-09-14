#pragma once

#include "device_rules.h"
#include "device_safety.h"

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
    CONFIGRET status_result = CR_SUCCESS;
    DeviceProtection protection = DeviceProtection::none;
};

enum class RemovalMethod {
    none,
    configuration_manager,
    setup_api,
    pnputil,
};

enum class RemovalEligibility {
    eligible,
    already_absent,
    present_or_unconfirmed,
    protected_device,
    invalid_id,
    lookup_error,
};

struct RemovalEligibilityResult {
    RemovalEligibility state = RemovalEligibility::lookup_error;
    CONFIGRET config_result = CR_SUCCESS;
    DeviceProtection protection = DeviceProtection::none;
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
    bool skipped_present = false;
    bool skipped_protected = false;
    RemovalMethod method = RemovalMethod::none;
};

using EnumerationProgress = std::function<void(size_t scanned, size_t found)>;

class DeviceManager {
public:
    static EnumerationResult EnumerateNonPresent(
        const DeviceFilters& filters, const EnumerationProgress& progress = {});
    static RemovalResult Remove(const DeviceRecord& device,
                                bool allow_protected = false);
    static RemovalEligibilityResult CheckRemovalEligibility(
        std::wstring_view instance_id, bool allow_protected = false);
    static CONFIGRET RequestReenumeration();
    static bool OpenProperties(HWND owner, const DeviceRecord& device);
};

}  // namespace sonic79

#include "device_manager.h"

#include <setupapi.h>

#include <iostream>

#include <algorithm>

namespace {

std::wstring FirstPresentDeviceId() {
    HDEVINFO devices = SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) {
        return {};
    }

    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    std::wstring id;
    if (SetupDiEnumDeviceInfo(devices, 0, &info)) {
        ULONG size = 0;
        if (CM_Get_Device_ID_Size(&size, info.DevInst, 0) == CR_SUCCESS) {
            id.resize(size + 1);
            if (CM_Get_Device_IDW(info.DevInst, id.data(),
                                 static_cast<ULONG>(id.size()), 0) == CR_SUCCESS) {
                id.resize(size);
            } else {
                id.clear();
            }
        }
    }
    SetupDiDestroyDeviceInfoList(devices);
    return id;
}

}  // namespace

int main() {
    const sonic79::EnumerationResult result =
        sonic79::DeviceManager::EnumerateNonPresent({});
    if (result.win32_error != ERROR_SUCCESS) {
        std::wcerr << L"Enumeration failed with Windows error "
                   << result.win32_error << L'\n';
        return 1;
    }

    const auto invalid = std::find_if(
        result.devices.begin(), result.devices.end(), [](const auto& device) {
            return (device.status_result != CR_NO_SUCH_DEVNODE &&
                    !(device.status_result == CR_SUCCESS &&
                      device.problem_code == CM_PROB_PHANTOM)) ||
                   device.instance_id.empty();
        });
    if (invalid != result.devices.end()) {
        std::wcerr << L"Enumeration returned an unconfirmed ghost device.\n";
        return 2;
    }

    const size_t protected_count = static_cast<size_t>(std::count_if(
        result.devices.begin(), result.devices.end(), [](const auto& device) {
            return sonic79::IsProtected(device.protection);
        }));
    const std::wstring present_id = FirstPresentDeviceId();
    if (present_id.empty()) {
        std::wcerr << L"Could not find a present device for the guard smoke test.\n";
        return 3;
    }
    const auto eligibility =
        sonic79::DeviceManager::CheckRemovalEligibility(present_id, true);
    if (eligibility.state !=
        sonic79::RemovalEligibility::present_or_unconfirmed) {
        std::wcerr << L"Removal guard did not reject a present device.\n";
        return 4;
    }

    std::wcout << L"Enumerated " << result.devices.size()
               << L" confirmed ghost device(s); " << protected_count
               << L" protected by affinity rules; present-device guard passed.\n";
    return 0;
}

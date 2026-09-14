#include "device_manager.h"

#include "com_port_manager.h"
#include "process_runner.h"
#include "win32_helpers.h"

#include <initguid.h>
#include <devpkey.h>
#include <setupapi.h>

#include <array>
#include <thread>
#include <vector>

namespace sonic79 {
namespace {

constexpr wchar_t kEnumPath[] = L"SYSTEM\\CurrentControlSet\\Enum\\";

class UniqueDeviceInfoSet {
public:
    explicit UniqueDeviceInfoSet(HDEVINFO value) : value_(value) {}
    ~UniqueDeviceInfoSet() {
        if (value_ != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(value_);
        }
    }
    HDEVINFO get() const { return value_; }

private:
    HDEVINFO value_ = INVALID_HANDLE_VALUE;
};

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

std::wstring QueryRegistryProperty(DEVINST dev_inst, ULONG property) {
    ULONG type = 0;
    ULONG size = 0;
    CONFIGRET result =
        CM_Get_DevNode_Registry_PropertyW(dev_inst, property, &type, nullptr, &size, 0);
    if (result != CR_BUFFER_SMALL && result != CR_SUCCESS) {
        return {};
    }
    if (size < sizeof(wchar_t) || (type != REG_SZ && type != REG_MULTI_SZ)) {
        return {};
    }

    std::vector<BYTE> bytes(size + sizeof(wchar_t), 0);
    result = CM_Get_DevNode_Registry_PropertyW(dev_inst, property, &type, bytes.data(),
                                               &size, 0);
    if (result != CR_SUCCESS) {
        return {};
    }
    return reinterpret_cast<const wchar_t*>(bytes.data());
}

std::wstring QueryInstanceId(DEVINST dev_inst) {
    ULONG character_count = 0;
    if (CM_Get_Device_ID_Size(&character_count, dev_inst, 0) != CR_SUCCESS) {
        return {};
    }
    std::vector<wchar_t> buffer(character_count + 1);
    if (CM_Get_Device_IDW(dev_inst, buffer.data(), static_cast<ULONG>(buffer.size()), 0) !=
        CR_SUCCESS) {
        return {};
    }
    return buffer.data();
}

std::wstring ReadComPort(std::wstring_view instance_id) {
    std::wstring path = kEnumPath;
    path.append(instance_id);
    path.append(L"\\Device Parameters");

    UniqueRegKey key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0,
                      KEY_QUERY_VALUE | KEY_WOW64_64KEY, key.put()) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD byte_count = 0;
    if (RegQueryValueExW(key.get(), L"PortName", nullptr, &type, nullptr, &byte_count) !=
            ERROR_SUCCESS ||
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

std::optional<FILETIME> ReadRegistryTimestamp(std::wstring_view instance_id) {
    std::wstring path = kEnumPath;
    path.append(instance_id);

    UniqueRegKey key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0,
                      KEY_QUERY_VALUE | KEY_WOW64_64KEY, key.put()) != ERROR_SUCCESS) {
        return std::nullopt;
    }

    FILETIME last_write{};
    if (RegQueryInfoKeyW(key.get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr, nullptr, &last_write) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return last_write;
}

std::optional<FILETIME> ReadLastConnected(DEVINST dev_inst,
                                          std::wstring_view instance_id) {
    FILETIME value{};
    ULONG size = sizeof(value);
    DEVPROPTYPE type = 0;
    if (CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_Device_LastRemovalDate, &type,
                                reinterpret_cast<PBYTE>(&value), &size, 0) == CR_SUCCESS &&
        type == DEVPROP_TYPE_FILETIME && size == sizeof(value)) {
        return value;
    }

    size = sizeof(value);
    type = 0;
    if (CM_Get_DevNode_PropertyW(dev_inst, &DEVPKEY_Device_LastArrivalDate, &type,
                                reinterpret_cast<PBYTE>(&value), &size, 0) == CR_SUCCESS &&
        type == DEVPROP_TYPE_FILETIME && size == sizeof(value)) {
        return value;
    }
    return ReadRegistryTimestamp(instance_id);
}

bool IsStillRegistered(std::wstring_view instance_id) {
    DEVINST located = 0;
    std::wstring mutable_id(instance_id);
    return CM_Locate_DevNodeW(&located, mutable_id.data(),
                              CM_LOCATE_DEVNODE_PHANTOM) == CR_SUCCESS;
}

bool WaitUntilUnregistered(std::wstring_view instance_id) {
    constexpr std::array<DWORD, 6> kRetryDelaysMs{0, 50, 100, 200, 400, 800};
    for (const DWORD delay : kRetryDelaysMs) {
        if (delay != 0) {
            Sleep(delay);
        }
        if (!IsStillRegistered(instance_id)) {
            return true;
        }
    }
    return false;
}

bool RemoveWithSetupApi(std::wstring_view instance_id, DWORD* error,
                        bool* reboot_required) {
    UniqueDeviceInfoSet set(SetupDiCreateDeviceInfoList(nullptr, nullptr));
    if (set.get() == INVALID_HANDLE_VALUE) {
        *error = GetLastError();
        return false;
    }

    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    std::wstring mutable_id(instance_id);
    if (!SetupDiOpenDeviceInfoW(set.get(), mutable_id.c_str(), nullptr, 0, &info)) {
        *error = GetLastError();
        return false;
    }

    SP_REMOVEDEVICE_PARAMS parameters{};
    parameters.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    parameters.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    parameters.Scope = DI_REMOVEDEVICE_GLOBAL;
    parameters.HwProfile = 0;
    if (!SetupDiSetClassInstallParamsW(
            set.get(), &info,
            reinterpret_cast<SP_CLASSINSTALL_HEADER*>(&parameters), sizeof(parameters)) ||
        !SetupDiCallClassInstaller(DIF_REMOVE, set.get(), &info)) {
        *error = GetLastError();
        return false;
    }

    SP_DEVINSTALL_PARAMS_W install_parameters{};
    install_parameters.cbSize = sizeof(install_parameters);
    if (SetupDiGetDeviceInstallParamsW(set.get(), &info, &install_parameters)) {
        *reboot_required =
            (install_parameters.Flags & (DI_NEEDREBOOT | DI_NEEDRESTART)) != 0;
    }
    return true;
}

std::wstring PnpUtilPath() {
    std::array<wchar_t, MAX_PATH> system_directory{};
    const UINT size = GetSystemDirectoryW(system_directory.data(),
                                          static_cast<UINT>(system_directory.size()));
    if (size == 0 || size >= system_directory.size()) {
        return {};
    }
    std::wstring path(system_directory.data(), size);
    path.append(L"\\pnputil.exe");
    return path;
}

bool RemoveWithPnpUtil(std::wstring_view instance_id, DWORD* error) {
    const std::wstring executable = PnpUtilPath();
    if (executable.empty()) {
        *error = GetLastError();
        return false;
    }

    const ProcessResult process = RunHiddenProcess(
        executable, {L"/remove-device", std::wstring(instance_id)}, 30000);
    if (!process.started || !process.completed) {
        *error = process.win32_error;
        return false;
    }
    if (process.exit_code != 0) {
        *error = ERROR_GEN_FAILURE;
        return false;
    }
    return true;
}

struct ValidatedRemovalTarget {
    RemovalEligibilityResult result;
    DEVINST dev_inst = 0;
};

ValidatedRemovalTarget ValidateRemovalTarget(std::wstring_view instance_id,
                                             bool allow_protected) {
    ValidatedRemovalTarget validation;
    if (instance_id.empty()) {
        validation.result.state = RemovalEligibility::invalid_id;
        validation.result.config_result = CR_INVALID_DEVICE_ID;
        return validation;
    }

    std::wstring mutable_id(instance_id);
    validation.result.config_result = CM_Locate_DevNodeW(
        &validation.dev_inst, mutable_id.data(), CM_LOCATE_DEVNODE_PHANTOM);
    if (validation.result.config_result == CR_NO_SUCH_DEVNODE) {
        validation.result.state = RemovalEligibility::already_absent;
        return validation;
    }
    if (validation.result.config_result != CR_SUCCESS) {
        validation.result.state = RemovalEligibility::lookup_error;
        return validation;
    }

    DWORD current_status = 0;
    DWORD current_problem = 0;
    validation.result.config_result = CM_Get_DevNode_Status(
        &current_status, &current_problem, validation.dev_inst, 0);
    const bool confirmed_phantom =
        validation.result.config_result == CR_NO_SUCH_DEVNODE ||
        (validation.result.config_result == CR_SUCCESS &&
         current_problem == CM_PROB_PHANTOM);
    if (!confirmed_phantom) {
        validation.result.state = RemovalEligibility::present_or_unconfirmed;
        return validation;
    }

    validation.result.protection = AnalyzeDeviceProtection(instance_id);
    if (IsProtected(validation.result.protection) && !allow_protected) {
        validation.result.state = RemovalEligibility::protected_device;
        return validation;
    }
    validation.result.state = RemovalEligibility::eligible;
    return validation;
}

}  // namespace

EnumerationResult DeviceManager::EnumerateNonPresent(
    const DeviceFilters& filters, const EnumerationProgress& progress) {
    EnumerationResult result;
    UniqueDeviceInfoSet set(
        SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES));
    if (set.get() == INVALID_HANDLE_VALUE) {
        result.win32_error = GetLastError();
        return result;
    }

    SP_DEVINFO_DATA info{};
    info.cbSize = sizeof(info);
    for (DWORD index = 0;; ++index) {
        if (!SetupDiEnumDeviceInfo(set.get(), index, &info)) {
            const DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_ITEMS) {
                result.win32_error = error;
            }
            break;
        }

        DWORD status = 0;
        DWORD problem = 0;
        const CONFIGRET status_result =
            CM_Get_DevNode_Status(&status, &problem, info.DevInst, 0);
        const bool non_present =
            (status_result == CR_SUCCESS && problem == CM_PROB_PHANTOM) ||
            status_result == CR_NO_SUCH_DEVNODE;
        if (!non_present) {
            if (progress) {
                progress(index + 1, result.devices.size());
            }
            continue;
        }

        DeviceRecord device;
        device.instance_id = QueryInstanceId(info.DevInst);
        if (device.instance_id.empty() ||
            !ShouldIncludeDevice(device.instance_id, filters)) {
            if (progress) {
                progress(index + 1, result.devices.size());
            }
            continue;
        }

        device.name = QueryRegistryProperty(info.DevInst, CM_DRP_FRIENDLYNAME);
        if (device.name.empty()) {
            device.name = QueryRegistryProperty(info.DevInst, CM_DRP_DEVICEDESC);
        }
        if (device.name.empty()) {
            device.name = device.instance_id;
        }
        device.class_name = QueryRegistryProperty(info.DevInst, CM_DRP_CLASS);
        device.enumerator = QueryRegistryProperty(info.DevInst, CM_DRP_ENUMERATOR_NAME);
        device.service = QueryRegistryProperty(info.DevInst, CM_DRP_SERVICE);
        device.com_port = ReadComPort(device.instance_id);
        device.last_connected = ReadLastConnected(info.DevInst, device.instance_id);
        device.status = status;
        device.problem_code = problem;
        device.status_result = status_result;
        device.protection = AnalyzeDeviceProtection(device.instance_id);
        result.devices.push_back(std::move(device));

        if (progress) {
            progress(index + 1, result.devices.size());
        }
    }
    return result;
}

RemovalResult DeviceManager::Remove(const DeviceRecord& device,
                                    bool allow_protected) {
    RemovalResult result;
    const ValidatedRemovalTarget validation =
        ValidateRemovalTarget(device.instance_id, allow_protected);
    result.config_result = validation.result.config_result;
    if (validation.result.state == RemovalEligibility::already_absent) {
        result.removed = true;
        return result;
    }
    if (validation.result.state == RemovalEligibility::present_or_unconfirmed) {
        result.skipped_present = true;
        return result;
    }
    if (validation.result.state == RemovalEligibility::protected_device) {
        result.skipped_protected = true;
        return result;
    }
    if (validation.result.state != RemovalEligibility::eligible) {
        return result;
    }

    result.config_result = CM_Uninstall_DevNode(validation.dev_inst, 0);
    if (result.config_result == CR_SUCCESS) {
        result.removed = true;
        result.method = RemovalMethod::configuration_manager;
    } else {
        result.removed =
            RemoveWithSetupApi(device.instance_id, &result.win32_error,
                               &result.reboot_required);
        if (result.removed) {
            result.method = RemovalMethod::setup_api;
        } else if (RemoveWithPnpUtil(device.instance_id, &result.win32_error)) {
            result.removed = true;
            result.method = RemovalMethod::pnputil;
        }
    }

    if (result.removed) {
        result.still_present = !WaitUntilUnregistered(device.instance_id);
        if (!result.still_present && !device.com_port.empty()) {
            result.com_reservation_released =
                ComPortManager::ReleaseReservationIfUnused(device.com_port,
                                                           device.instance_id);
        }
    }
    return result;
}

RemovalEligibilityResult DeviceManager::CheckRemovalEligibility(
    std::wstring_view instance_id, bool allow_protected) {
    return ValidateRemovalTarget(instance_id, allow_protected).result;
}

CONFIGRET DeviceManager::RequestReenumeration() {
    DEVINST root = 0;
    CONFIGRET result = CM_Locate_DevNodeW(&root, nullptr,
                                          CM_LOCATE_DEVNODE_NORMAL);
    if (result != CR_SUCCESS) {
        return result;
    }
    return CM_Reenumerate_DevNode(root, CM_REENUMERATE_SYNCHRONOUS);
}

bool DeviceManager::OpenProperties(HWND owner, const DeviceRecord& device) {
    std::wstring parameters =
        L"devmgr.dll,DeviceProperties_RunDLL /MachineName \"\" /DeviceID \"";
    parameters.append(device.instance_id);
    parameters.append(L"\"");
    return LaunchControlPanelItem(owner, L"rundll32.exe", parameters.c_str());
}

}  // namespace sonic79

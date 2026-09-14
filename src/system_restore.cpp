#include "system_restore.h"

#include "win32_helpers.h"

#include <srrestoreptapi.h>

#include <algorithm>
#include <vector>

namespace sonic79 {
namespace {

constexpr wchar_t kSystemRestoreKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore";
constexpr wchar_t kFrequencyValue[] = L"SystemRestorePointCreationFrequency";

class UniqueModule {
public:
    explicit UniqueModule(HMODULE module) : module_(module) {}
    ~UniqueModule() {
        if (module_ != nullptr) {
            FreeLibrary(module_);
        }
    }
    HMODULE get() const { return module_; }

private:
    HMODULE module_ = nullptr;
};

class RestoreFrequencyOverride {
public:
    bool Apply() {
        const LSTATUS open_result = RegOpenKeyExW(
            HKEY_LOCAL_MACHINE, kSystemRestoreKey, 0,
            KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, &key_);
        if (open_result != ERROR_SUCCESS) {
            error_ = static_cast<DWORD>(open_result);
            return false;
        }

        DWORD size = 0;
        LSTATUS query_result = RegQueryValueExW(
            key_, kFrequencyValue, nullptr, &previous_type_, nullptr, &size);
        if (query_result == ERROR_SUCCESS) {
            previous_data_.resize(size);
            query_result = RegQueryValueExW(
                key_, kFrequencyValue, nullptr, &previous_type_,
                previous_data_.empty() ? nullptr : previous_data_.data(), &size);
            if (query_result != ERROR_SUCCESS) {
                error_ = static_cast<DWORD>(query_result);
                return false;
            }
            had_value_ = true;
        } else if (query_result != ERROR_FILE_NOT_FOUND) {
            error_ = static_cast<DWORD>(query_result);
            return false;
        }

        const DWORD zero = 0;
        const LSTATUS set_result =
            RegSetValueExW(key_, kFrequencyValue, 0, REG_DWORD,
                           reinterpret_cast<const BYTE*>(&zero), sizeof(zero));
        if (set_result != ERROR_SUCCESS) {
            error_ = static_cast<DWORD>(set_result);
            return false;
        }
        applied_ = true;
        return true;
    }

    ~RestoreFrequencyOverride() {
        if (key_ == nullptr) {
            return;
        }
        if (applied_) {
            if (had_value_) {
                RegSetValueExW(key_, kFrequencyValue, 0, previous_type_,
                               previous_data_.empty() ? nullptr : previous_data_.data(),
                               static_cast<DWORD>(previous_data_.size()));
            } else {
                RegDeleteValueW(key_, kFrequencyValue);
            }
        }
        RegCloseKey(key_);
    }

    DWORD error() const { return error_; }

private:
    HKEY key_ = nullptr;
    DWORD previous_type_ = REG_NONE;
    DWORD error_ = ERROR_SUCCESS;
    std::vector<BYTE> previous_data_;
    bool had_value_ = false;
    bool applied_ = false;
};

}  // namespace

RestorePointResult SystemRestore::Create(std::wstring_view description) {
    RestorePointResult result;
    if (!IsAdministrator()) {
        result.error = ERROR_ACCESS_DENIED;
        result.message = L"Administrator privileges are required.";
        return result;
    }

    UniqueModule module(LoadLibraryW(L"srclient.dll"));
    if (module.get() == nullptr) {
        result.error = GetLastError();
        result.message = L"System Restore is not available: " +
                         FormatWin32Error(result.error);
        return result;
    }

    using SetRestorePoint = BOOL(WINAPI*)(PRESTOREPOINTINFOW, PSTATEMGRSTATUS);
    const auto set_restore_point = reinterpret_cast<SetRestorePoint>(
        GetProcAddress(module.get(), "SRSetRestorePointW"));
    if (set_restore_point == nullptr) {
        result.error = ERROR_PROC_NOT_FOUND;
        result.message = L"System Restore is not available on this Windows installation.";
        return result;
    }

    RestoreFrequencyOverride frequency_override;
    if (!frequency_override.Apply()) {
        result.error = frequency_override.error();
        result.message = L"Cannot temporarily change the restore-point frequency: " +
                         FormatWin32Error(result.error);
        return result;
    }

    RESTOREPOINTINFOW begin{};
    begin.dwEventType = BEGIN_SYSTEM_CHANGE;
    begin.dwRestorePtType = DEVICE_DRIVER_INSTALL;
    const size_t copy_length =
        std::min(description.size(), static_cast<size_t>(MAX_DESC_W - 1));
    description.copy(begin.szDescription, copy_length);
    begin.szDescription[copy_length] = L'\0';

    STATEMGRSTATUS begin_status{};
    if (!set_restore_point(&begin, &begin_status) || begin_status.nStatus != ERROR_SUCCESS) {
        result.error = begin_status.nStatus != ERROR_SUCCESS ? begin_status.nStatus
                                                             : GetLastError();
        result.message = L"Unable to create a System Restore point: " +
                         FormatWin32Error(result.error);
        return result;
    }

    RESTOREPOINTINFOW end{};
    end.dwEventType = END_SYSTEM_CHANGE;
    end.dwRestorePtType = DEVICE_DRIVER_INSTALL;
    end.llSequenceNumber = begin_status.llSequenceNumber;
    STATEMGRSTATUS end_status{};
    if (!set_restore_point(&end, &end_status) || end_status.nStatus != ERROR_SUCCESS) {
        result.error = end_status.nStatus != ERROR_SUCCESS ? end_status.nStatus
                                                           : GetLastError();
        result.message = L"The restore point started but could not be finalized: " +
                         FormatWin32Error(result.error);
        return result;
    }

    result.success = true;
    result.message = L"System Restore point successfully created.";
    return result;
}

}  // namespace sonic79

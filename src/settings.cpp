#include "settings.h"

#include "win32_helpers.h"

#include <shlobj.h>

#include <algorithm>
#include <system_error>

namespace sonic79 {
namespace {

constexpr wchar_t kSection[] = L"Settings";

bool ReadBool(const std::filesystem::path& path, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(kSection, key, fallback ? 1 : 0,
                                 path.c_str()) != 0;
}

int ReadInt(const std::filesystem::path& path, const wchar_t* key, int fallback,
            int minimum, int maximum) {
    const int value = GetPrivateProfileIntW(kSection, key, fallback, path.c_str());
    return std::clamp(value, minimum, maximum);
}

bool WriteInt(const std::filesystem::path& path, const wchar_t* key, int value) {
    const std::wstring text = std::to_wstring(value);
    return WritePrivateProfileStringW(kSection, key, text.c_str(), path.c_str()) != FALSE;
}

std::filesystem::path ProgramDataSettingsPath() {
    PWSTR program_data = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, nullptr,
                             &program_data) != S_OK ||
        program_data == nullptr) {
        return {};
    }
    std::filesystem::path path(program_data);
    CoTaskMemFree(program_data);
    return path / L"Sonic79Reconstructed" / L"Sonic79Reconstructed.ini";
}

bool IsNewer(const std::filesystem::path& left,
             const std::filesystem::path& right) {
    std::error_code error;
    const auto left_time = std::filesystem::last_write_time(left, error);
    if (error) {
        return false;
    }
    error.clear();
    const auto right_time = std::filesystem::last_write_time(right, error);
    return !error && left_time > right_time;
}

bool SaveToPath(const std::filesystem::path& path,
                const AppSettings& settings) {
    bool success = true;
    if (settings.has_window_placement) {
        success &= WritePrivateProfileStructW(
                       kSection, L"WindowPlacement",
                       const_cast<WINDOWPLACEMENT*>(&settings.window_placement),
                       sizeof(settings.window_placement), path.c_str()) != FALSE;
    }

    success &= WriteInt(path, L"SortCol", settings.sort_column);
    success &= WriteInt(path, L"SortDir", settings.sort_ascending ? 1 : 0);
    success &= WriteInt(path, L"ShowEnumerator", settings.show_enumerator ? 1 : 0);
    success &= WriteInt(path, L"ShowService", settings.show_service ? 1 : 0);
    success &= WriteInt(path, L"ShowComPort", settings.show_com_port ? 1 : 0);
    success &= WriteInt(path, L"ShowDeviceId", settings.show_device_id ? 1 : 0);
    success &= WriteInt(path, L"DarkMode", settings.dark_mode ? 1 : 0);
    success &= WriteInt(path, L"AlwaysOnTop", settings.always_on_top ? 1 : 0);
    success &= WriteInt(path, L"ListDevsRoot", settings.filters.include_root ? 1 : 0);
    success &= WriteInt(path, L"ListDevsSwd", settings.filters.include_swd ? 1 : 0);
    success &= WriteInt(path, L"ListDevsSw", settings.filters.include_sw ? 1 : 0);
    success &= WriteInt(path, L"DeviceNameColumnWidth", settings.device_name_width);
    success &= WriteInt(path, L"LastUsedColumnWidth", settings.last_connected_width);
    success &= WriteInt(path, L"ClassColumnWidth", settings.class_width);
    success &= WriteInt(path, L"SafetyColumnWidth", settings.safety_width);
    success &= WriteInt(path, L"EnumeratorColumnWidth", settings.enumerator_width);
    success &= WriteInt(path, L"ServiceColumnWidth", settings.service_width);
    success &= WriteInt(path, L"ComPortColumnWidth", settings.com_port_width);
    success &= WriteInt(path, L"DeviceIdColumnWidth", settings.device_id_width);
    return success;
}

}  // namespace

SettingsStore::SettingsStore() {
    std::filesystem::path sidecar = GetExecutablePath();
    sidecar.replace_extension(L".ini");
    fallback_path_ = ProgramDataSettingsPath();

    std::error_code error;
    const bool sidecar_exists = std::filesystem::exists(sidecar, error);
    error.clear();
    const bool fallback_exists = !fallback_path_.empty() &&
                                 std::filesystem::exists(fallback_path_, error);
    if (fallback_exists &&
        (!sidecar_exists || IsNewer(fallback_path_, sidecar))) {
        path_ = fallback_path_;
    } else {
        path_ = sidecar;
    }
}

AppSettings SettingsStore::Load() const {
    AppSettings settings;
    settings.has_window_placement =
        GetPrivateProfileStructW(kSection, L"WindowPlacement",
                                 &settings.window_placement,
                                 sizeof(settings.window_placement), path_.c_str()) != FALSE;

    settings.sort_column = ReadInt(path_, L"SortCol", 0, 0, 7);
    settings.sort_ascending = ReadBool(path_, L"SortDir", true);
    settings.show_enumerator = ReadBool(path_, L"ShowEnumerator", true);
    settings.show_service = ReadBool(path_, L"ShowService", true);
    settings.show_com_port = ReadBool(path_, L"ShowComPort", true);
    settings.show_device_id = ReadBool(path_, L"ShowDeviceId", false);
    settings.dark_mode = ReadBool(path_, L"DarkMode", false);
    settings.always_on_top = ReadBool(path_, L"AlwaysOnTop", false);
    settings.filters.include_root = ReadBool(path_, L"ListDevsRoot", false);
    settings.filters.include_swd = ReadBool(path_, L"ListDevsSwd", false);
    settings.filters.include_sw = ReadBool(path_, L"ListDevsSw", false);

    settings.device_name_width = ReadInt(path_, L"DeviceNameColumnWidth", 330, 40, 1200);
    settings.last_connected_width = ReadInt(path_, L"LastUsedColumnWidth", 130, 40, 1200);
    settings.class_width = ReadInt(path_, L"ClassColumnWidth", 120, 40, 1200);
    settings.safety_width = ReadInt(path_, L"SafetyColumnWidth", 180, 40, 1200);
    settings.enumerator_width = ReadInt(path_, L"EnumeratorColumnWidth", 110, 40, 1200);
    settings.service_width = ReadInt(path_, L"ServiceColumnWidth", 110, 40, 1200);
    settings.com_port_width = ReadInt(path_, L"ComPortColumnWidth", 80, 40, 1200);
    settings.device_id_width = ReadInt(path_, L"DeviceIdColumnWidth", 360, 40, 1600);
    return settings;
}

bool SettingsStore::Save(const AppSettings& settings) const {
    if (SaveToPath(path_, settings)) {
        return true;
    }
    if (fallback_path_.empty() || path_ == fallback_path_) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(fallback_path_.parent_path(), error);
    if (error || !SaveToPath(fallback_path_, settings)) {
        return false;
    }
    path_ = fallback_path_;
    return true;
}

}  // namespace sonic79

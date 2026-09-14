#include "settings.h"

#include "win32_helpers.h"

#include <shlobj.h>

#include <algorithm>

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

}  // namespace

SettingsStore::SettingsStore() {
    path_ = GetExecutablePath();
    path_.replace_extension(L".ini");
}

AppSettings SettingsStore::Load() const {
    AppSettings settings;
    settings.has_window_placement =
        GetPrivateProfileStructW(kSection, L"WindowPlacement",
                                 &settings.window_placement,
                                 sizeof(settings.window_placement), path_.c_str()) != FALSE;

    settings.sort_column = ReadInt(path_, L"SortCol", 0, 0, 6);
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
    settings.enumerator_width = ReadInt(path_, L"EnumeratorColumnWidth", 110, 40, 1200);
    settings.service_width = ReadInt(path_, L"ServiceColumnWidth", 110, 40, 1200);
    settings.com_port_width = ReadInt(path_, L"ComPortColumnWidth", 80, 40, 1200);
    settings.device_id_width = ReadInt(path_, L"DeviceIdColumnWidth", 360, 40, 1600);
    return settings;
}

bool SettingsStore::Save(const AppSettings& settings) const {
    bool success = true;
    if (settings.has_window_placement) {
        success &= WritePrivateProfileStructW(kSection, L"WindowPlacement",
                                               const_cast<WINDOWPLACEMENT*>(
                                                   &settings.window_placement),
                                               sizeof(settings.window_placement),
                                               path_.c_str()) != FALSE;
    }

    success &= WriteInt(path_, L"SortCol", settings.sort_column);
    success &= WriteInt(path_, L"SortDir", settings.sort_ascending ? 1 : 0);
    success &= WriteInt(path_, L"ShowEnumerator", settings.show_enumerator ? 1 : 0);
    success &= WriteInt(path_, L"ShowService", settings.show_service ? 1 : 0);
    success &= WriteInt(path_, L"ShowComPort", settings.show_com_port ? 1 : 0);
    success &= WriteInt(path_, L"ShowDeviceId", settings.show_device_id ? 1 : 0);
    success &= WriteInt(path_, L"DarkMode", settings.dark_mode ? 1 : 0);
    success &= WriteInt(path_, L"AlwaysOnTop", settings.always_on_top ? 1 : 0);
    success &= WriteInt(path_, L"ListDevsRoot", settings.filters.include_root ? 1 : 0);
    success &= WriteInt(path_, L"ListDevsSwd", settings.filters.include_swd ? 1 : 0);
    success &= WriteInt(path_, L"ListDevsSw", settings.filters.include_sw ? 1 : 0);
    success &= WriteInt(path_, L"DeviceNameColumnWidth", settings.device_name_width);
    success &= WriteInt(path_, L"LastUsedColumnWidth", settings.last_connected_width);
    success &= WriteInt(path_, L"ClassColumnWidth", settings.class_width);
    success &= WriteInt(path_, L"EnumeratorColumnWidth", settings.enumerator_width);
    success &= WriteInt(path_, L"ServiceColumnWidth", settings.service_width);
    success &= WriteInt(path_, L"ComPortColumnWidth", settings.com_port_width);
    success &= WriteInt(path_, L"DeviceIdColumnWidth", settings.device_id_width);
    return success;
}

}  // namespace sonic79

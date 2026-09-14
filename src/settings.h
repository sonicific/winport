#pragma once

#include "device_rules.h"

#include <windows.h>

#include <filesystem>

namespace sonic79 {

struct AppSettings {
    WINDOWPLACEMENT window_placement{sizeof(WINDOWPLACEMENT)};
    bool has_window_placement = false;

    int sort_column = 0;
    bool sort_ascending = true;
    bool show_enumerator = true;
    bool show_service = true;
    bool show_com_port = true;
    bool show_device_id = false;
    bool dark_mode = false;
    bool always_on_top = false;
    DeviceFilters filters{};

    int device_name_width = 330;
    int last_connected_width = 130;
    int class_width = 120;
    int enumerator_width = 110;
    int service_width = 110;
    int com_port_width = 80;
    int device_id_width = 360;
};

class SettingsStore {
public:
    SettingsStore();

    AppSettings Load() const;
    bool Save(const AppSettings& settings) const;
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace sonic79

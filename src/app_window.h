#pragma once

#include "device_manager.h"
#include "settings.h"

#include <windows.h>

#include <future>
#include <vector>

namespace sonic79 {

class AppWindow {
public:
    int Run(HINSTANCE instance, int show_command, bool smoke_test = false);

private:
    enum class DeviceColumn {
        name = 0,
        last_connected = 1,
        class_name = 2,
        enumerator = 3,
        service = 4,
        com_port = 5,
        device_id = 6,
        safety = 7,
    };

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam,
                                            LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    bool RegisterWindowClass() const;
    void CreateControls();
    void CreateApplicationMenu();
    void LayoutControls(int width, int height) const;
    void BuildColumns();
    void SaveColumnWidths();
    void BeginRefreshDevices();
    void FinishRefreshDevices();
    bool ValidateSmokeState() const;
    void PopulateList();
    void SortDevices();
    void UpdateStatus();
    void UpdateMenuState();
    void ApplyTheme();
    void SaveSettings();
    void HandleCommand(int command_id);
    void HandleNotify(const NMHDR* notification);
    void ShowContextMenu(POINT screen_point);
    void SelectAll();
    void SelectNone();
    void RemoveSelected();
    void OpenSelectedProperties();
    void CopySelected();
    std::vector<size_t> SelectedDeviceIndices() const;
    std::wstring ColumnText(const DeviceRecord& device, DeviceColumn column) const;
    int ColumnWidth(DeviceColumn column) const;
    void SetColumnWidth(DeviceColumn column, int width);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND list_ = nullptr;
    HWND status_ = nullptr;
    HMENU menu_ = nullptr;
    HACCEL accelerators_ = nullptr;

    SettingsStore settings_store_;
    AppSettings settings_;
    std::vector<DeviceRecord> devices_;
    std::vector<DeviceColumn> visible_columns_;
    bool administrator_ = false;
    bool smoke_test_ = false;
    bool allow_protected_removal_ = false;
    bool scan_in_progress_ = false;
    bool refresh_pending_ = false;
    bool closing_ = false;
    int exit_code_ = 0;
    size_t removed_session_count_ = 0;
    std::future<EnumerationResult> scan_future_;
};

}  // namespace sonic79

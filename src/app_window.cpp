#include "app_window.h"

#include "bluetooth_dialog.h"
#include "commands.h"
#include "system_restore.h"
#include "win32_helpers.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <sstream>
#include <string>

namespace sonic79 {
namespace {

constexpr wchar_t kWindowClass[] = L"Sonic79ReconstructedMainWindow";
constexpr wchar_t kBaseTitle[] = L"Sonic79 Reconstructed (x64)";
constexpr UINT_PTR kDeviceRefreshTimer = 1;
constexpr UINT_PTR kSmokeTestTimer = 2;
constexpr UINT_PTR kScanPollTimer = 3;

bool SetClipboardText(HWND owner, std::wstring_view text) {
    if (!OpenClipboard(owner)) {
        return false;
    }
    EmptyClipboard();

    const size_t byte_count = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, byte_count);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }

    void* destination = GlobalLock(memory);
    if (destination == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    memcpy(destination, text.data(), text.size() * sizeof(wchar_t));
    static_cast<wchar_t*>(destination)[text.size()] = L'\0';
    GlobalUnlock(memory);

    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

int CompareStrings(std::wstring_view left, std::wstring_view right) {
    const int result = CompareStringOrdinal(
        left.data(), static_cast<int>(left.size()), right.data(),
        static_cast<int>(right.size()), TRUE);
    if (result == CSTR_LESS_THAN) {
        return -1;
    }
    if (result == CSTR_GREATER_THAN) {
        return 1;
    }
    return 0;
}

std::wstring ConfigResultText(CONFIGRET result) {
    std::wostringstream stream;
    stream << L"CONFIGRET 0x" << std::hex << std::uppercase << result;
    return stream.str();
}

}  // namespace

int AppWindow::Run(HINSTANCE instance, int show_command, bool smoke_test) {
    instance_ = instance;
    smoke_test_ = smoke_test;
    settings_ = settings_store_.Load();
    administrator_ = IsAdministrator();
    if (!RegisterWindowClass()) {
        return 1;
    }

    std::wstring title = kBaseTitle;
    if (!administrator_) {
        title += L"  [Restricted]";
    }
    window_ = CreateWindowExW(
        0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1080, 650, nullptr, nullptr, instance_, this);
    if (window_ == nullptr) {
        return 1;
    }

    if (settings_.has_window_placement) {
        settings_.window_placement.length = sizeof(WINDOWPLACEMENT);
        SetWindowPlacement(window_, &settings_.window_placement);
    }
    ShowWindow(window_, show_command);
    UpdateWindow(window_);

    const std::array<ACCEL, 3> accelerator_data{{
        {FVIRTKEY, VK_F5, static_cast<WORD>(command::refresh)},
        {FVIRTKEY, VK_DELETE, static_cast<WORD>(command::remove_selected)},
        {FVIRTKEY | FCONTROL, 'A', static_cast<WORD>(command::select_all)},
    }};
    accelerators_ = CreateAcceleratorTableW(
        const_cast<ACCEL*>(accelerator_data.data()),
        static_cast<int>(accelerator_data.size()));

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (accelerators_ == nullptr ||
            !TranslateAcceleratorW(window_, accelerators_, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (accelerators_ != nullptr) {
        DestroyAcceleratorTable(accelerators_);
    }
    return static_cast<int>(message.wParam);
}

bool AppWindow::RegisterWindowClass() const {
    WNDCLASSEXW window_class{sizeof(window_class)};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance_;
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = kWindowClass;
    return RegisterClassExW(&window_class) != 0 ||
           GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

LRESULT CALLBACK AppWindow::WindowProcedure(HWND window, UINT message, WPARAM wparam,
                                             LPARAM lparam) {
    AppWindow* self = reinterpret_cast<AppWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<AppWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr ? self->HandleMessage(message, wparam, lparam)
                           : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            CreateControls();
            CreateApplicationMenu();
            ApplyTheme();
            if (settings_.always_on_top) {
                SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            BeginRefreshDevices();
            return 0;
        case WM_SIZE:
            LayoutControls(LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_COMMAND:
            HandleCommand(LOWORD(wparam));
            return 0;
        case WM_NOTIFY:
            HandleNotify(reinterpret_cast<NMHDR*>(lparam));
            return 0;
        case WM_DEVICECHANGE:
            KillTimer(window_, kDeviceRefreshTimer);
            SetTimer(window_, kDeviceRefreshTimer, 700, nullptr);
            return TRUE;
        case WM_TIMER:
            if (wparam == kDeviceRefreshTimer) {
                KillTimer(window_, kDeviceRefreshTimer);
                BeginRefreshDevices();
            } else if (wparam == kSmokeTestTimer) {
                KillTimer(window_, kSmokeTestTimer);
                PostMessageW(window_, WM_CLOSE, 0, 0);
            } else if (wparam == kScanPollTimer) {
                FinishRefreshDevices();
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize = POINT{680, 380};
            return 0;
        }
        case WM_CLOSE:
            closing_ = true;
            KillTimer(window_, kDeviceRefreshTimer);
            KillTimer(window_, kScanPollTimer);
            KillTimer(window_, kSmokeTestTimer);
            SaveSettings();
            DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(exit_code_);
            return 0;
        default:
            return DefWindowProcW(window_, message, wparam, lparam);
    }
}

void AppWindow::CreateApplicationMenu() {
    menu_ = CreateMenu();

    HMENU file_menu = CreatePopupMenu();
    AppendMenuW(file_menu, MF_STRING, command::refresh, L"&Refresh\tF5");
    AppendMenuW(file_menu, MF_STRING, command::restart_elevated,
                L"Restart 'As &Administrator'");
    AppendMenuW(file_menu, MF_STRING, command::create_restore_point,
                L"&Create System Restore Point");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, command::exit, L"E&xit\tAlt+F4");

    HMENU devices_menu = CreatePopupMenu();
    AppendMenuW(devices_menu, MF_STRING, command::select_all, L"Select &all\tCtrl+A");
    AppendMenuW(devices_menu, MF_STRING, command::select_none, L"Select &none");
    AppendMenuW(devices_menu, MF_STRING, command::remove_selected,
                L"&Remove selected\tDelete");
    AppendMenuW(devices_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(devices_menu, MF_STRING, command::allow_protected_removal,
                L"Allow removal of &protected devices (session only)");

    HMENU columns_menu = CreatePopupMenu();
    AppendMenuW(columns_menu, MF_STRING, command::show_enumerator, L"&Enumerator");
    AppendMenuW(columns_menu, MF_STRING, command::show_service, L"&Service");
    AppendMenuW(columns_menu, MF_STRING, command::show_com_port, L"&COM Port");
    AppendMenuW(columns_menu, MF_STRING, command::show_device_id, L"&Device ID");

    HMENU filters_menu = CreatePopupMenu();
    AppendMenuW(filters_menu, MF_STRING, command::show_root, L"Show &ROOT\\*");
    AppendMenuW(filters_menu, MF_STRING, command::show_swd, L"Show SW&D\\*");
    AppendMenuW(filters_menu, MF_STRING, command::show_sw, L"Show &SW\\{*");

    HMENU options_menu = CreatePopupMenu();
    AppendMenuW(options_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(columns_menu),
                L"&Columns");
    AppendMenuW(options_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(filters_menu),
                L"&Devices");
    AppendMenuW(options_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(options_menu, MF_STRING, command::dark_mode, L"Half-dar&k mode");
    AppendMenuW(options_menu, MF_STRING, command::always_on_top, L"&Always On Top");

    HMENU tools_menu = CreatePopupMenu();
    AppendMenuW(tools_menu, MF_STRING, command::bluetooth, L"&Bluetooth Devices");
    AppendMenuW(tools_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(tools_menu, MF_STRING, command::device_manager, L"De&vice Manager");
    AppendMenuW(tools_menu, MF_STRING, command::disk_management, L"&Disk Management");
    AppendMenuW(tools_menu, MF_STRING, command::event_viewer, L"&Event Viewer");
    AppendMenuW(tools_menu, MF_STRING, command::network_adapters, L"&Network Adapters");

    HMENU help_menu = CreatePopupMenu();
    AppendMenuW(help_menu, MF_STRING, command::about, L"&About");

    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"&File");
    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(devices_menu), L"&Devices");
    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(options_menu), L"&Options");
    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(tools_menu), L"&Tools");
    AppendMenuW(menu_, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), L"&Help");
    SetMenu(window_, menu_);
    UpdateMenuState();
}

void AppWindow::CreateControls() {
    list_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    ListView_SetExtendedListViewStyle(
        list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES |
                   LVS_EX_HEADERDRAGDROP);
    status_ = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE,
                              0, 0, 0, 0, window_, nullptr, instance_, nullptr);
    BuildColumns();
}

void AppWindow::LayoutControls(int width, int height) const {
    SendMessageW(status_, WM_SIZE, 0, 0);
    RECT status_rect{};
    GetWindowRect(status_, &status_rect);
    const int status_height = status_rect.bottom - status_rect.top;
    MoveWindow(list_, 0, 0, width, std::max(0, height - status_height), TRUE);
}

void AppWindow::BuildColumns() {
    while (ListView_DeleteColumn(list_, 0)) {
    }
    visible_columns_.clear();
    visible_columns_.push_back(DeviceColumn::name);
    visible_columns_.push_back(DeviceColumn::last_connected);
    visible_columns_.push_back(DeviceColumn::class_name);
    visible_columns_.push_back(DeviceColumn::safety);
    if (settings_.show_enumerator) {
        visible_columns_.push_back(DeviceColumn::enumerator);
    }
    if (settings_.show_service) {
        visible_columns_.push_back(DeviceColumn::service);
    }
    if (settings_.show_com_port) {
        visible_columns_.push_back(DeviceColumn::com_port);
    }
    if (settings_.show_device_id) {
        visible_columns_.push_back(DeviceColumn::device_id);
    }

    for (int index = 0; index < static_cast<int>(visible_columns_.size()); ++index) {
        const DeviceColumn logical_column = visible_columns_[index];
        const wchar_t* title = L"";
        switch (logical_column) {
            case DeviceColumn::name:
                title = L"Device Name";
                break;
            case DeviceColumn::last_connected:
                title = L"Last connected";
                break;
            case DeviceColumn::class_name:
                title = L"Class";
                break;
            case DeviceColumn::safety:
                title = L"Safety";
                break;
            case DeviceColumn::enumerator:
                title = L"Enumerator";
                break;
            case DeviceColumn::service:
                title = L"Service";
                break;
            case DeviceColumn::com_port:
                title = L"COM Port";
                break;
            case DeviceColumn::device_id:
                title = L"Device ID";
                break;
        }

        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<wchar_t*>(title);
        column.cx = ColumnWidth(logical_column);
        column.iSubItem = index;
        ListView_InsertColumn(list_, index, &column);
    }
}

void AppWindow::SaveColumnWidths() {
    for (int index = 0; index < static_cast<int>(visible_columns_.size()); ++index) {
        SetColumnWidth(visible_columns_[index], ListView_GetColumnWidth(list_, index));
    }
}

void AppWindow::BeginRefreshDevices() {
    if (scan_in_progress_) {
        refresh_pending_ = true;
        return;
    }

    scan_in_progress_ = true;
    refresh_pending_ = false;
    SendMessageW(status_, SB_SETTEXTW, 0,
                 reinterpret_cast<LPARAM>(L"Scanning Windows device tree..."));
    UpdateMenuState();
    const DeviceFilters filters = settings_.filters;
    scan_future_ = std::async(std::launch::async, [filters] {
        return DeviceManager::EnumerateNonPresent(filters);
    });
    SetTimer(window_, kScanPollTimer, 50, nullptr);
}

void AppWindow::FinishRefreshDevices() {
    if (!scan_in_progress_ || !scan_future_.valid() ||
        scan_future_.wait_for(std::chrono::milliseconds(0)) !=
            std::future_status::ready) {
        return;
    }

    KillTimer(window_, kScanPollTimer);
    EnumerationResult result;
    try {
        result = scan_future_.get();
    } catch (...) {
        scan_in_progress_ = false;
        exit_code_ = 2;
        MessageBoxW(window_, L"An unexpected error stopped device enumeration.",
                    L"Device enumeration failed", MB_OK | MB_ICONERROR);
        UpdateMenuState();
        if (smoke_test_) {
            SetTimer(window_, kSmokeTestTimer, 50, nullptr);
        }
        return;
    }
    scan_in_progress_ = false;
    if (closing_) {
        return;
    }
    devices_ = result.devices;
    SortDevices();
    PopulateList();

    if (result.win32_error != ERROR_SUCCESS) {
        MessageBoxW(window_, FormatWin32Error(result.win32_error).c_str(),
                    L"Device enumeration failed", MB_OK | MB_ICONERROR);
    }
    if (refresh_pending_) {
        BeginRefreshDevices();
    } else if (smoke_test_) {
        exit_code_ = ValidateSmokeState() ? 0 : 2;
        SetTimer(window_, kSmokeTestTimer, 50, nullptr);
    }
}

bool AppWindow::ValidateSmokeState() const {
    if (!IsWindow(window_) || !IsWindow(list_) || !IsWindow(status_) ||
        menu_ == nullptr) {
        return false;
    }
    const HWND header = ListView_GetHeader(list_);
    if (!IsWindow(header) ||
        Header_GetItemCount(header) != static_cast<int>(visible_columns_.size()) ||
        ListView_GetItemCount(list_) != static_cast<int>(devices_.size())) {
        return false;
    }
    if (GetMenuState(menu_, command::allow_protected_removal, MF_BYCOMMAND) ==
        static_cast<UINT>(-1)) {
        return false;
    }
    return std::all_of(devices_.begin(), devices_.end(), [](const auto& device) {
        return !device.instance_id.empty() &&
               (device.status_result == CR_NO_SUCH_DEVNODE ||
                (device.status_result == CR_SUCCESS &&
                 device.problem_code == CM_PROB_PHANTOM));
    });
}

void AppWindow::PopulateList() {
    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list_);
    for (size_t index = 0; index < devices_.size(); ++index) {
        const DeviceRecord& device = devices_[index];
        const std::wstring name = ColumnText(device, visible_columns_.front());
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(index);
        item.pszText = const_cast<wchar_t*>(name.c_str());
        item.lParam = static_cast<LPARAM>(index);
        const int row = ListView_InsertItem(list_, &item);

        for (int column = 1; column < static_cast<int>(visible_columns_.size()); ++column) {
            const std::wstring text = ColumnText(device, visible_columns_[column]);
            ListView_SetItemText(list_, row, column,
                                 const_cast<wchar_t*>(text.c_str()));
        }
    }
    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list_, nullptr, TRUE);
    UpdateStatus();
    UpdateMenuState();
}

void AppWindow::SortDevices() {
    const DeviceColumn column = static_cast<DeviceColumn>(settings_.sort_column);
    const bool ascending = settings_.sort_ascending;
    std::stable_sort(devices_.begin(), devices_.end(),
                     [this, column, ascending](const DeviceRecord& left,
                                               const DeviceRecord& right) {
                         int comparison = 0;
                         if (column == DeviceColumn::last_connected) {
                             if (left.last_connected && right.last_connected) {
                                 comparison = CompareFileTime(&*left.last_connected,
                                                              &*right.last_connected);
                             } else if (left.last_connected) {
                                 comparison = 1;
                             } else if (right.last_connected) {
                                 comparison = -1;
                             }
                         } else {
                             comparison = CompareStrings(ColumnText(left, column),
                                                         ColumnText(right, column));
                         }
                         if (comparison == 0) {
                             comparison = CompareStrings(left.instance_id, right.instance_id);
                         }
                         return ascending ? comparison < 0 : comparison > 0;
                     });
}

void AppWindow::UpdateStatus() {
    const size_t selection_count = SelectedDeviceIndices().size();
    const size_t protected_count = static_cast<size_t>(std::count_if(
        devices_.begin(), devices_.end(), [](const DeviceRecord& device) {
            return IsProtected(device.protection);
        }));
    std::wstring text = L"  Non-present devices: " +
                        std::to_wstring(devices_.size()) + L"    Selected: " +
                        std::to_wstring(selection_count) + L"    Protected: " +
                        std::to_wstring(protected_count) + L"    Removed this session: " +
                        std::to_wstring(removed_session_count_) + L"    (F5 refreshes)";
    SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

void AppWindow::UpdateMenuState() {
    const auto check = [this](int id, bool enabled) {
        CheckMenuItem(menu_, id, MF_BYCOMMAND | (enabled ? MF_CHECKED : MF_UNCHECKED));
    };
    check(command::show_enumerator, settings_.show_enumerator);
    check(command::show_service, settings_.show_service);
    check(command::show_com_port, settings_.show_com_port);
    check(command::show_device_id, settings_.show_device_id);
    check(command::show_root, settings_.filters.include_root);
    check(command::show_swd, settings_.filters.include_swd);
    check(command::show_sw, settings_.filters.include_sw);
    check(command::dark_mode, settings_.dark_mode);
    check(command::always_on_top, settings_.always_on_top);
    check(command::allow_protected_removal, allow_protected_removal_);

    EnableMenuItem(menu_, command::remove_selected,
                   MF_BYCOMMAND |
                       (scan_in_progress_ || SelectedDeviceIndices().empty()
                            ? MF_GRAYED
                            : MF_ENABLED));
    EnableMenuItem(menu_, command::refresh,
                   MF_BYCOMMAND | (scan_in_progress_ ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu_, command::restart_elevated,
                   MF_BYCOMMAND | (administrator_ ? MF_GRAYED : MF_ENABLED));
    DrawMenuBar(window_);
}

void AppWindow::ApplyTheme() {
    const BOOL dark = settings_.dark_mode ? TRUE : FALSE;
    DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark));
    SetWindowTheme(list_, settings_.dark_mode ? L"DarkMode_Explorer" : L"Explorer",
                   nullptr);
    SetWindowTheme(status_, settings_.dark_mode ? L"DarkMode_Explorer" : nullptr,
                   nullptr);

    const COLORREF background = settings_.dark_mode ? RGB(32, 32, 32) :
                                                      GetSysColor(COLOR_WINDOW);
    const COLORREF foreground = settings_.dark_mode ? RGB(238, 238, 238) :
                                                      GetSysColor(COLOR_WINDOWTEXT);
    ListView_SetBkColor(list_, background);
    ListView_SetTextBkColor(list_, background);
    ListView_SetTextColor(list_, foreground);
    InvalidateRect(window_, nullptr, TRUE);
}

void AppWindow::SaveSettings() {
    if (smoke_test_) {
        return;
    }
    SaveColumnWidths();
    settings_.window_placement.length = sizeof(WINDOWPLACEMENT);
    settings_.has_window_placement =
        GetWindowPlacement(window_, &settings_.window_placement) != FALSE;
    settings_store_.Save(settings_);
}

void AppWindow::HandleCommand(int command_id) {
    switch (command_id) {
        case command::refresh:
            BeginRefreshDevices();
            break;
        case command::restart_elevated:
            if (RestartElevated(window_)) {
                PostMessageW(window_, WM_CLOSE, 0, 0);
            }
            break;
        case command::create_restore_point: {
            const RestorePointResult result =
                SystemRestore::Create(L"Before ghost-device cleanup");
            MessageBoxW(window_, result.message.c_str(), L"System Restore",
                        MB_OK | (result.success ? MB_ICONINFORMATION : MB_ICONERROR));
            break;
        }
        case command::exit:
            PostMessageW(window_, WM_CLOSE, 0, 0);
            break;
        case command::select_all:
            SelectAll();
            break;
        case command::select_none:
            SelectNone();
            break;
        case command::remove_selected:
            RemoveSelected();
            break;
        case command::properties:
            OpenSelectedProperties();
            break;
        case command::copy:
            CopySelected();
            break;
        case command::show_enumerator:
        case command::show_service:
        case command::show_com_port:
        case command::show_device_id:
            SaveColumnWidths();
            if (command_id == command::show_enumerator) {
                settings_.show_enumerator = !settings_.show_enumerator;
            } else if (command_id == command::show_service) {
                settings_.show_service = !settings_.show_service;
            } else if (command_id == command::show_com_port) {
                settings_.show_com_port = !settings_.show_com_port;
            } else {
                settings_.show_device_id = !settings_.show_device_id;
            }
            BuildColumns();
            PopulateList();
            break;
        case command::show_root:
        case command::show_swd:
        case command::show_sw:
            if (command_id == command::show_root) {
                settings_.filters.include_root = !settings_.filters.include_root;
            } else if (command_id == command::show_swd) {
                settings_.filters.include_swd = !settings_.filters.include_swd;
            } else {
                settings_.filters.include_sw = !settings_.filters.include_sw;
            }
            UpdateMenuState();
            BeginRefreshDevices();
            break;
        case command::dark_mode:
            settings_.dark_mode = !settings_.dark_mode;
            UpdateMenuState();
            ApplyTheme();
            break;
        case command::always_on_top:
            settings_.always_on_top = !settings_.always_on_top;
            SetWindowPos(window_, settings_.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
                         0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            UpdateMenuState();
            break;
        case command::allow_protected_removal:
            if (allow_protected_removal_) {
                allow_protected_removal_ = false;
            } else if (MessageBoxW(
                           window_,
                           L"Protected entries include ROOT/software-enumerated devices "
                           L"and devices with explicit interrupt affinity. Removing them "
                           L"can disrupt Windows, drivers, or tuned performance.\r\n\r\n"
                           L"Enable protected-device removal for this session?",
                           L"Advanced safety override",
                           MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
                allow_protected_removal_ = true;
            }
            UpdateMenuState();
            break;
        case command::bluetooth:
            ShowBluetoothDialog(window_);
            break;
        case command::device_manager:
            LaunchControlPanelItem(window_, L"devmgmt.msc");
            break;
        case command::disk_management:
            LaunchControlPanelItem(window_, L"diskmgmt.msc");
            break;
        case command::event_viewer:
            LaunchControlPanelItem(window_, L"eventvwr.msc");
            break;
        case command::network_adapters:
            LaunchControlPanelItem(window_, L"control.exe", L"ncpa.cpl");
            break;
        case command::about:
            MessageBoxW(
                window_,
                L"Sonic79 Reconstructed 1.0\r\n\r\n"
                L"Independent clean-room implementation of a Windows ghost-device "
                L"cleanup utility.\r\n\r\n"
                L"The reference executable's code sections match Device Cleanup Tool "
                L"1.5.1 by Uwe Sieber; no original source, icon, signature or modified "
                L"resource is included here.",
                L"About Sonic79 Reconstructed", MB_OK | MB_ICONINFORMATION);
            break;
        default:
            break;
    }
}

void AppWindow::HandleNotify(const NMHDR* notification) {
    if (notification == nullptr || notification->hwndFrom != list_) {
        return;
    }
    switch (notification->code) {
        case LVN_ITEMCHANGED:
            UpdateStatus();
            UpdateMenuState();
            break;
        case LVN_COLUMNCLICK: {
            const auto* list_view = reinterpret_cast<const NMLISTVIEW*>(notification);
            if (list_view->iSubItem < 0 ||
                list_view->iSubItem >= static_cast<int>(visible_columns_.size())) {
                break;
            }
            const int clicked =
                static_cast<int>(visible_columns_[list_view->iSubItem]);
            if (settings_.sort_column == clicked) {
                settings_.sort_ascending = !settings_.sort_ascending;
            } else {
                settings_.sort_column = clicked;
                settings_.sort_ascending = true;
            }
            SortDevices();
            PopulateList();
            break;
        }
        case NM_DBLCLK:
            OpenSelectedProperties();
            break;
        case NM_RCLICK: {
            POINT point{};
            GetCursorPos(&point);
            ShowContextMenu(point);
            break;
        }
        default:
            break;
    }
}

void AppWindow::ShowContextMenu(POINT screen_point) {
    HMENU popup = CreatePopupMenu();
    AppendMenuW(popup, MF_STRING, command::remove_selected, L"&Remove Device");
    AppendMenuW(popup, MF_STRING, command::properties, L"&Properties");
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup, MF_STRING, command::copy, L"&Copy");
    const bool has_selection = !SelectedDeviceIndices().empty();
    if (!has_selection) {
        EnableMenuItem(popup, command::remove_selected, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(popup, command::properties, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(popup, command::copy, MF_BYCOMMAND | MF_GRAYED);
    }
    const int selected = TrackPopupMenu(
        popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen_point.x, screen_point.y, 0,
        window_, nullptr);
    DestroyMenu(popup);
    if (selected != 0) {
        HandleCommand(selected);
    }
}

void AppWindow::SelectAll() {
    ListView_SetItemState(list_, -1, LVIS_SELECTED, LVIS_SELECTED);
    UpdateStatus();
    UpdateMenuState();
}

void AppWindow::SelectNone() {
    ListView_SetItemState(list_, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    UpdateStatus();
    UpdateMenuState();
}

void AppWindow::RemoveSelected() {
    const std::vector<size_t> indices = SelectedDeviceIndices();
    if (indices.empty()) {
        return;
    }
    if (!administrator_) {
        if (MessageBoxW(window_,
                        L"Administrator privileges are required to remove devices. "
                        L"Restart elevated now?",
                        L"Elevation required", MB_YESNO | MB_ICONINFORMATION) == IDYES &&
            RestartElevated(window_)) {
            PostMessageW(window_, WM_CLOSE, 0, 0);
        }
        return;
    }

    const size_t protected_count = static_cast<size_t>(std::count_if(
        indices.begin(), indices.end(), [this](size_t index) {
            return index < devices_.size() && IsProtected(devices_[index].protection);
        }));
    if (protected_count != 0 && !allow_protected_removal_) {
        MessageBoxW(
            window_,
            (std::to_wstring(protected_count) +
             L" selected device(s) are protected. Deselect them, or explicitly enable "
             L"the session-only override under Devices after reviewing the Safety column.")
                .c_str(),
            L"Protected devices were not removed", MB_OK | MB_ICONWARNING);
        return;
    }
    if (protected_count != 0 &&
        MessageBoxW(
            window_,
            (L"You selected " + std::to_wstring(protected_count) +
             L" protected device(s). This can disrupt Windows, drivers, or explicit "
             L"interrupt-affinity tuning. Continue anyway?")
                .c_str(),
            L"Confirm protected-device removal",
            MB_YESNO | MB_ICONSTOP | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    const std::wstring prompt =
        L"Remove " + std::to_wstring(indices.size()) +
        L" non-present device(s)?\r\n\r\n"
        L"Windows will detect them as new if attached again. Any COM reservation used "
        L"only by a removed device may also be cleared.";
    if (MessageBoxW(window_, prompt.c_str(), L"Confirm device removal",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }

    const RestorePointResult restore =
        SystemRestore::Create(L"Before ghost-device cleanup");
    if (!restore.success) {
        const std::wstring warning =
            restore.message + L"\r\n\r\nContinue without a restore point?";
        if (MessageBoxW(window_, warning.c_str(), L"System Restore warning",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            return;
        }
    }

    std::vector<DeviceRecord> selected_devices;
    selected_devices.reserve(indices.size());
    for (const size_t index : indices) {
        if (index < devices_.size()) {
            selected_devices.push_back(devices_[index]);
        }
    }

    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    std::wstring failures;
    size_t removed_count = 0;
    bool reboot_required = false;
    for (const DeviceRecord& device : selected_devices) {
        SendMessageW(status_, SB_SETTEXTW, 0,
                     reinterpret_cast<LPARAM>((L"Removing: " + device.name).c_str()));
        const RemovalResult removal =
            DeviceManager::Remove(device, allow_protected_removal_);
        reboot_required |= removal.reboot_required;
        if (removal.removed && !removal.still_present) {
            ++removed_count;
            continue;
        }

        failures += device.name + L" (" + device.instance_id + L"): ";
        if (removal.skipped_present) {
            failures += L"skipped because it is no longer a confirmed ghost device";
        } else if (removal.skipped_protected) {
            failures += L"skipped because its protection state changed";
        } else if (removal.still_present) {
            failures += L"device removal was requested but the device is still registered";
        } else if (removal.win32_error != ERROR_SUCCESS) {
            failures += FormatWin32Error(removal.win32_error);
        } else {
            failures += ConfigResultText(removal.config_result);
        }
        failures += L"\r\n";
    }
    if (!selected_devices.empty()) {
        DeviceManager::RequestReenumeration();
    }
    removed_session_count_ += removed_count;
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    BeginRefreshDevices();

    std::wstring summary = std::to_wstring(removed_count) + L" of " +
                           std::to_wstring(selected_devices.size()) +
                           L" device(s) removed.";
    if (reboot_required) {
        summary += L"\r\nA Windows restart is required.";
    }
    if (!failures.empty()) {
        summary += L"\r\n\r\n" + failures;
    }
    MessageBoxW(window_, summary.c_str(), L"Device cleanup result",
                MB_OK | (failures.empty() ? MB_ICONINFORMATION : MB_ICONWARNING));
}

void AppWindow::OpenSelectedProperties() {
    const std::vector<size_t> indices = SelectedDeviceIndices();
    if (indices.empty()) {
        return;
    }
    if (indices.size() > 3) {
        const std::wstring prompt = L"This opens " + std::to_wstring(indices.size()) +
                                    L" property dialogs. Continue?";
        if (MessageBoxW(window_, prompt.c_str(), L"Open device properties",
                        MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
    }
    for (const size_t index : indices) {
        if (index < devices_.size()) {
            DeviceManager::OpenProperties(window_, devices_[index]);
        }
    }
}

void AppWindow::CopySelected() {
    const std::vector<size_t> indices = SelectedDeviceIndices();
    if (indices.empty()) {
        return;
    }

    std::wstring text;
    for (size_t column = 0; column < visible_columns_.size(); ++column) {
        if (column > 0) {
            text += L'\t';
        }
        switch (visible_columns_[column]) {
            case DeviceColumn::name:
                text += L"Device Name";
                break;
            case DeviceColumn::last_connected:
                text += L"Last connected";
                break;
            case DeviceColumn::class_name:
                text += L"Class";
                break;
            case DeviceColumn::safety:
                text += L"Safety";
                break;
            case DeviceColumn::enumerator:
                text += L"Enumerator";
                break;
            case DeviceColumn::service:
                text += L"Service";
                break;
            case DeviceColumn::com_port:
                text += L"COM Port";
                break;
            case DeviceColumn::device_id:
                text += L"Device ID";
                break;
        }
    }
    text += L"\r\n";

    for (const size_t index : indices) {
        if (index >= devices_.size()) {
            continue;
        }
        for (size_t column = 0; column < visible_columns_.size(); ++column) {
            if (column > 0) {
                text += L'\t';
            }
            text += ColumnText(devices_[index], visible_columns_[column]);
        }
        text += L"\r\n";
    }

    if (!SetClipboardText(window_, text)) {
        MessageBoxW(window_, FormatWin32Error(GetLastError()).c_str(),
                    L"Clipboard error", MB_OK | MB_ICONERROR);
    }
}

std::vector<size_t> AppWindow::SelectedDeviceIndices() const {
    std::vector<size_t> indices;
    if (list_ == nullptr) {
        return indices;
    }
    for (int row = ListView_GetNextItem(list_, -1, LVNI_SELECTED); row >= 0;
         row = ListView_GetNextItem(list_, row, LVNI_SELECTED)) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (ListView_GetItem(list_, &item) && item.lParam >= 0) {
            indices.push_back(static_cast<size_t>(item.lParam));
        }
    }
    return indices;
}

std::wstring AppWindow::ColumnText(const DeviceRecord& device,
                                   DeviceColumn column) const {
    switch (column) {
        case DeviceColumn::name:
            return device.name;
        case DeviceColumn::last_connected:
            return device.last_connected ? FormatFileTime(*device.last_connected) : L"";
        case DeviceColumn::class_name:
            return device.class_name;
        case DeviceColumn::safety:
            return DescribeDeviceProtection(device.protection);
        case DeviceColumn::enumerator:
            return device.enumerator;
        case DeviceColumn::service:
            return device.service;
        case DeviceColumn::com_port:
            return device.com_port;
        case DeviceColumn::device_id:
            return device.instance_id;
    }
    return {};
}

int AppWindow::ColumnWidth(DeviceColumn column) const {
    switch (column) {
        case DeviceColumn::name:
            return settings_.device_name_width;
        case DeviceColumn::last_connected:
            return settings_.last_connected_width;
        case DeviceColumn::class_name:
            return settings_.class_width;
        case DeviceColumn::safety:
            return settings_.safety_width;
        case DeviceColumn::enumerator:
            return settings_.enumerator_width;
        case DeviceColumn::service:
            return settings_.service_width;
        case DeviceColumn::com_port:
            return settings_.com_port_width;
        case DeviceColumn::device_id:
            return settings_.device_id_width;
    }
    return 100;
}

void AppWindow::SetColumnWidth(DeviceColumn column, int width) {
    switch (column) {
        case DeviceColumn::name:
            settings_.device_name_width = width;
            break;
        case DeviceColumn::last_connected:
            settings_.last_connected_width = width;
            break;
        case DeviceColumn::class_name:
            settings_.class_width = width;
            break;
        case DeviceColumn::safety:
            settings_.safety_width = width;
            break;
        case DeviceColumn::enumerator:
            settings_.enumerator_width = width;
            break;
        case DeviceColumn::service:
            settings_.service_width = width;
            break;
        case DeviceColumn::com_port:
            settings_.com_port_width = width;
            break;
        case DeviceColumn::device_id:
            settings_.device_id_width = width;
            break;
    }
}

}  // namespace sonic79

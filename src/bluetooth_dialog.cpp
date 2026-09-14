#include "bluetooth_dialog.h"

#include "bluetooth_manager.h"
#include "win32_helpers.h"

#include <commctrl.h>

#include <array>
#include <string>
#include <vector>

namespace sonic79 {
namespace {

constexpr wchar_t kClassName[] = L"Sonic79BluetoothDialog";
constexpr int kRemoveButtonId = 1001;
constexpr int kCloseButtonId = 1002;

class BluetoothDialog {
public:
    explicit BluetoothDialog(HWND parent) : parent_(parent) {}

    void ShowModal() {
        RegisterWindowClass();
        window_ = CreateWindowExW(
            WS_EX_DLGMODALFRAME, kClassName, L"Non-connected Bluetooth Devices",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX, CW_USEDEFAULT,
            CW_USEDEFAULT, 780, 430, parent_, nullptr, GetModuleHandleW(nullptr), this);
        if (window_ == nullptr) {
            return;
        }

        RECT window_rect{};
        RECT parent_rect{};
        GetWindowRect(window_, &window_rect);
        GetWindowRect(parent_, &parent_rect);
        const int width = window_rect.right - window_rect.left;
        const int height = window_rect.bottom - window_rect.top;
        SetWindowPos(window_, nullptr,
                     parent_rect.left + (parent_rect.right - parent_rect.left - width) / 2,
                     parent_rect.top + (parent_rect.bottom - parent_rect.top - height) / 2,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);

        EnableWindow(parent_, FALSE);
        ShowWindow(window_, SW_SHOW);
        UpdateWindow(window_);

        MSG message{};
        int message_result = 1;
        while (!closed_ &&
               (message_result = static_cast<int>(
                    GetMessageW(&message, nullptr, 0, 0))) > 0) {
            if (!IsDialogMessageW(window_, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }

        EnableWindow(parent_, TRUE);
        SetForegroundWindow(parent_);
        if (message_result == 0) {
            PostQuitMessage(static_cast<int>(message.wParam));
        }
    }

private:
    static void RegisterWindowClass() {
        WNDCLASSEXW window_class{sizeof(window_class)};
        window_class.lpfnWndProc = WindowProcedure;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = kClassName;
        RegisterClassExW(&window_class);
    }

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam,
                                            LPARAM lparam) {
        BluetoothDialog* self = reinterpret_cast<BluetoothDialog*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<BluetoothDialog*>(create->lpCreateParams);
            self->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(self));
        }
        return self != nullptr ? self->HandleMessage(message, wparam, lparam)
                               : DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
            case WM_CREATE:
                CreateControls();
                Refresh();
                return 0;
            case WM_SIZE:
                Layout(LOWORD(lparam), HIWORD(lparam));
                return 0;
            case WM_COMMAND:
                if (LOWORD(wparam) == kRemoveButtonId) {
                    RemoveSelected();
                } else if (LOWORD(wparam) == kCloseButtonId) {
                    DestroyWindow(window_);
                }
                return 0;
            case WM_NOTIFY:
                if (reinterpret_cast<NMHDR*>(lparam)->hwndFrom == list_ &&
                    reinterpret_cast<NMHDR*>(lparam)->code == LVN_ITEMCHANGED) {
                    EnableWindow(remove_button_,
                                 ListView_GetSelectedCount(list_) != 0);
                }
                return 0;
            case WM_CLOSE:
                DestroyWindow(window_);
                return 0;
            case WM_DESTROY:
                closed_ = true;
                return 0;
            default:
                return DefWindowProcW(window_, message, wparam, lparam);
        }
    }

    void CreateControls() {
        list_ = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, window_, nullptr, GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(
            list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

        const std::array<std::pair<const wchar_t*, int>, 4> columns{{
            {L"Device Name", 260},
            {L"Address", 140},
            {L"Class", 190},
            {L"Last used", 145},
        }};
        for (int index = 0; index < static_cast<int>(columns.size()); ++index) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(columns[index].first);
            column.cx = columns[index].second;
            ListView_InsertColumn(list_, index, &column);
        }

        remove_button_ = CreateWindowExW(
            0, WC_BUTTONW, L"Remove selected", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 0, 0, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRemoveButtonId)),
            GetModuleHandleW(nullptr), nullptr);
        close_button_ = CreateWindowExW(
            0, WC_BUTTONW, L"Close",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0,
            window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCloseButtonId)),
            GetModuleHandleW(nullptr), nullptr);
        EnableWindow(remove_button_, FALSE);
    }

    void Layout(int width, int height) const {
        constexpr int margin = 10;
        constexpr int button_width = 130;
        constexpr int button_height = 28;
        constexpr int gap = 8;
        MoveWindow(list_, margin, margin, width - margin * 2,
                   height - margin * 3 - button_height, TRUE);
        MoveWindow(close_button_, width - margin - button_width,
                   height - margin - button_height, button_width, button_height, TRUE);
        MoveWindow(remove_button_, width - margin - button_width * 2 - gap,
                   height - margin - button_height, button_width, button_height, TRUE);
    }

    void Refresh() {
        const BluetoothEnumerationResult result =
            BluetoothManager::EnumerateDisconnected();
        devices_ = result.devices;
        ListView_DeleteAllItems(list_);

        for (size_t index = 0; index < devices_.size(); ++index) {
            const BluetoothDeviceRecord& device = devices_[index];
            LVITEMW item{};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = static_cast<int>(index);
            item.pszText = const_cast<wchar_t*>(device.name.c_str());
            item.lParam = static_cast<LPARAM>(index);
            const int row = ListView_InsertItem(list_, &item);

            const std::wstring address = BluetoothManager::FormatAddress(device.address);
            const std::wstring device_class =
                BluetoothManager::FormatClass(device.class_of_device);
            const std::wstring last_used = FormatSystemTime(device.last_used);
            ListView_SetItemText(list_, row, 1, const_cast<wchar_t*>(address.c_str()));
            ListView_SetItemText(list_, row, 2,
                                 const_cast<wchar_t*>(device_class.c_str()));
            ListView_SetItemText(list_, row, 3,
                                 const_cast<wchar_t*>(last_used.c_str()));
        }

        if (result.error != ERROR_SUCCESS) {
            MessageBoxW(window_, FormatWin32Error(result.error).c_str(),
                        L"Bluetooth enumeration failed", MB_OK | MB_ICONERROR);
        }
    }

    void RemoveSelected() {
        std::vector<BLUETOOTH_ADDRESS> selected;
        for (int row = ListView_GetNextItem(list_, -1, LVNI_SELECTED); row >= 0;
             row = ListView_GetNextItem(list_, row, LVNI_SELECTED)) {
            LVITEMW item{};
            item.mask = LVIF_PARAM;
            item.iItem = row;
            if (ListView_GetItem(list_, &item) &&
                static_cast<size_t>(item.lParam) < devices_.size()) {
                selected.push_back(devices_[item.lParam].address);
            }
        }
        if (selected.empty()) {
            return;
        }

        const std::wstring prompt =
            L"Remove " + std::to_wstring(selected.size()) +
            L" remembered Bluetooth device(s)? They must be paired again before reuse.";
        if (MessageBoxW(window_, prompt.c_str(), L"Confirm Bluetooth removal",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            return;
        }

        std::wstring errors;
        for (const BLUETOOTH_ADDRESS& address : selected) {
            const DWORD error = BluetoothManager::Remove(address);
            if (error != ERROR_SUCCESS) {
                errors += BluetoothManager::FormatAddress(address) + L": " +
                          FormatWin32Error(error) + L"\r\n";
            }
        }
        Refresh();
        if (!errors.empty()) {
            MessageBoxW(window_, errors.c_str(), L"Some devices were not removed",
                        MB_OK | MB_ICONERROR);
        }
    }

    HWND parent_ = nullptr;
    HWND window_ = nullptr;
    HWND list_ = nullptr;
    HWND remove_button_ = nullptr;
    HWND close_button_ = nullptr;
    bool closed_ = false;
    std::vector<BluetoothDeviceRecord> devices_;
};

}  // namespace

void ShowBluetoothDialog(HWND parent) {
    BluetoothDialog(parent).ShowModal();
}

}  // namespace sonic79

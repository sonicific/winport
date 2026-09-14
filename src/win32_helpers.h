#pragma once

#include <windows.h>

#include <optional>
#include <string>

namespace sonic79 {

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) : handle_(handle) {}
    ~UniqueHandle();

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept;
    UniqueHandle& operator=(UniqueHandle&& other) noexcept;

    HANDLE get() const { return handle_; }
    explicit operator bool() const {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }
    HANDLE release();
    void reset(HANDLE handle = nullptr);

private:
    HANDLE handle_ = nullptr;
};

std::wstring GetExecutablePath();
std::wstring FormatWin32Error(DWORD error);
std::wstring FormatFileTime(const FILETIME& utc_file_time);
std::wstring FormatSystemTime(const SYSTEMTIME& system_time);
bool IsAdministrator();
bool RestartElevated(HWND owner);
bool LaunchControlPanelItem(HWND owner, const wchar_t* file, const wchar_t* parameters = nullptr);

}  // namespace sonic79

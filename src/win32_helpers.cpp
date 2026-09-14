#include "win32_helpers.h"

#include <shellapi.h>

#include <array>
#include <vector>

namespace sonic79 {

UniqueHandle::~UniqueHandle() {
    reset();
}

UniqueHandle::UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}

UniqueHandle& UniqueHandle::operator=(UniqueHandle&& other) noexcept {
    if (this != &other) {
        reset(other.release());
    }
    return *this;
}

HANDLE UniqueHandle::release() {
    HANDLE result = handle_;
    handle_ = nullptr;
    return result;
}

void UniqueHandle::reset(HANDLE handle) {
    if (*this) {
        CloseHandle(handle_);
    }
    handle_ = handle;
}

std::wstring GetExecutablePath() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring FormatWin32Error(DWORD error) {
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&message), 0, nullptr);

    if (length == 0 || message == nullptr) {
        return L"Windows error " + std::to_wstring(error);
    }

    std::wstring result(message, length);
    LocalFree(message);
    while (!result.empty() &&
           (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) {
        result.pop_back();
    }
    return result;
}

std::wstring FormatFileTime(const FILETIME& utc_file_time) {
    FILETIME local_file_time{};
    SYSTEMTIME system_time{};
    if (!FileTimeToLocalFileTime(&utc_file_time, &local_file_time) ||
        !FileTimeToSystemTime(&local_file_time, &system_time)) {
        return {};
    }
    return FormatSystemTime(system_time);
}

std::wstring FormatSystemTime(const SYSTEMTIME& system_time) {
    if (system_time.wYear == 0 || system_time.wMonth == 0 || system_time.wDay == 0) {
        return {};
    }
    std::array<wchar_t, 32> buffer{};
    swprintf_s(buffer.data(), buffer.size(), L"%04u-%02u-%02u %02u:%02u",
               system_time.wYear, system_time.wMonth, system_time.wDay,
               system_time.wHour, system_time.wMinute);
    return buffer.data();
}

bool IsAdministrator() {
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &administrators)) {
        return false;
    }

    BOOL member = FALSE;
    const BOOL checked = CheckTokenMembership(nullptr, administrators, &member);
    FreeSid(administrators);
    return checked && member;
}

bool RestartElevated(HWND owner) {
    const std::wstring executable = GetExecutablePath();
    if (executable.empty()) {
        return false;
    }

    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(owner, L"runas", executable.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
}

bool LaunchControlPanelItem(HWND owner, const wchar_t* file, const wchar_t* parameters) {
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(owner, L"open", file, parameters, nullptr, SW_SHOWNORMAL));
    if (result > 32) {
        return true;
    }

    MessageBoxW(owner,
                (L"Unable to open the requested Windows tool. Error " +
                 std::to_wstring(result))
                    .c_str(),
                L"Sonic79 Reconstructed", MB_OK | MB_ICONERROR);
    return false;
}

}  // namespace sonic79

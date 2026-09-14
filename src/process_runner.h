#pragma once

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

namespace sonic79 {

struct ProcessResult {
    bool started = false;
    bool completed = false;
    DWORD exit_code = static_cast<DWORD>(-1);
    DWORD win32_error = ERROR_SUCCESS;
};

ProcessResult RunHiddenProcess(const std::wstring& application,
                               const std::vector<std::wstring>& arguments,
                               DWORD timeout_ms);

std::wstring QuoteWindowsArgument(std::wstring_view argument);

}  // namespace sonic79

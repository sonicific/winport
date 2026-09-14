#include "process_runner.h"

#include "win32_helpers.h"

#include <vector>

namespace sonic79 {

std::wstring QuoteWindowsArgument(std::wstring_view argument) {
    if (!argument.empty() &&
        argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
        return std::wstring(argument);
    }

    std::wstring quoted = L"\"";
    size_t slashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++slashes;
            continue;
        }
        if (character == L'\"') {
            quoted.append(slashes * 2 + 1, L'\\');
            quoted.push_back(L'\"');
        } else {
            quoted.append(slashes, L'\\');
            quoted.push_back(character);
        }
        slashes = 0;
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

ProcessResult RunHiddenProcess(const std::wstring& application,
                               const std::vector<std::wstring>& arguments,
                               DWORD timeout_ms) {
    ProcessResult result;
    std::wstring command_line = QuoteWindowsArgument(application);
    for (const auto& argument : arguments) {
        command_line.push_back(L' ');
        command_line.append(QuoteWindowsArgument(argument));
    }

    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(application.c_str(), mutable_command.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        result.win32_error = GetLastError();
        return result;
    }

    result.started = true;
    UniqueHandle process_handle(process.hProcess);
    UniqueHandle thread_handle(process.hThread);
    const DWORD wait_result = WaitForSingleObject(process_handle.get(), timeout_ms);
    if (wait_result == WAIT_TIMEOUT) {
        result.win32_error = ERROR_TIMEOUT;
        return result;
    }
    if (wait_result != WAIT_OBJECT_0) {
        result.win32_error = GetLastError();
        return result;
    }

    result.completed = true;
    if (!GetExitCodeProcess(process_handle.get(), &result.exit_code)) {
        result.win32_error = GetLastError();
    }
    return result;
}

}  // namespace sonic79

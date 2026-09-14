#pragma once

#include <windows.h>

#include <string>

namespace sonic79 {

struct RestorePointResult {
    bool success = false;
    DWORD error = ERROR_SUCCESS;
    std::wstring message;
};

class SystemRestore {
public:
    static RestorePointResult Create(std::wstring_view description);
};

}  // namespace sonic79

#include "app_window.h"

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

#include <string_view>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line,
                    int show_command) {
    INITCOMMONCONTROLSEX controls{sizeof(controls)};
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    sonic79::AppWindow app;
    const bool smoke_test =
        std::wstring_view(command_line).find(L"--smoke-test") !=
        std::wstring_view::npos;
    const int result =
        app.Run(instance, smoke_test ? SW_HIDE : show_command, smoke_test);
    if (SUCCEEDED(com_result)) {
        CoUninitialize();
    }
    return result;
}

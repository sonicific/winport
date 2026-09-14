cmake_minimum_required(VERSION 3.24)

project(Sonic79Reconstructed VERSION 1.0.0 LANGUAGES CXX RC)

add_executable(Sonic79Reconstructed WIN32
    resources/app.rc
    src/app_window.cpp
    src/bluetooth_dialog.cpp
    src/bluetooth_manager.cpp
    src/com_port_manager.cpp
    src/device_manager.cpp
    src/device_rules.cpp
    src/main.cpp
    src/settings.cpp
    src/system_restore.cpp
    src/win32_helpers.cpp
)

target_compile_features(Sonic79Reconstructed PRIVATE cxx_std_20)
target_compile_definitions(Sonic79Reconstructed PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0601
)
target_include_directories(Sonic79Reconstructed PRIVATE src resources)
target_link_libraries(Sonic79Reconstructed PRIVATE
    Advapi32
    Bthprops
    Comctl32
    Cfgmgr32
    Dwmapi
    Ole32
    Setupapi
    Shell32
    User32
    Uxtheme
)

if(MSVC)
    target_compile_options(Sonic79Reconstructed PRIVATE /W4 /permissive- /EHsc)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(core_tests
        tests/core_tests.cpp
        src/device_rules.cpp
    )
    target_compile_features(core_tests PRIVATE cxx_std_20)
    target_compile_definitions(core_tests PRIVATE UNICODE _UNICODE NOMINMAX)
    target_include_directories(core_tests PRIVATE src)
    add_test(NAME core_tests COMMAND core_tests)

    add_executable(enumeration_smoke
        tests/enumeration_smoke.cpp
        src/com_port_manager.cpp
        src/device_manager.cpp
        src/device_rules.cpp
        src/win32_helpers.cpp
    )
    target_compile_features(enumeration_smoke PRIVATE cxx_std_20)
    target_compile_definitions(enumeration_smoke PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
    )
    target_include_directories(enumeration_smoke PRIVATE src)
    target_link_libraries(enumeration_smoke PRIVATE
        Advapi32 Cfgmgr32 Setupapi Shell32 User32
    )
    add_test(NAME enumeration_smoke COMMAND enumeration_smoke)
endif()

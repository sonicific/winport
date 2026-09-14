#include "device_rules.h"

#include <cassert>

int main() {
    using namespace sonic79;

    assert(ClassifyProtectedDevice(L"ROOT\\LEGACY_TEST") == ProtectedDeviceKind::root);
    assert(ClassifyProtectedDevice(L"htree\\root\\0") == ProtectedDeviceKind::root);
    assert(ClassifyProtectedDevice(L"SWD\\PRINTENUM\\1") == ProtectedDeviceKind::swd);
    assert(ClassifyProtectedDevice(L"sw\\{abc}") == ProtectedDeviceKind::sw);
    assert(ClassifyProtectedDevice(L"USB\\VID_1234") == ProtectedDeviceKind::none);

    const DeviceFilters defaults{};
    assert(!ShouldIncludeDevice(L"ROOT\\LEGACY_TEST", defaults));
    assert(!ShouldIncludeDevice(L"SWD\\PRINTENUM\\1", defaults));
    assert(ShouldIncludeDevice(L"USB\\VID_1234", defaults));

    assert(ParseComPortNumber(L"COM1") == 1u);
    assert(ParseComPortNumber(L"com256") == 256u);
    assert(!ParseComPortNumber(L"COM0"));
    assert(!ParseComPortNumber(L"COM7x"));
    assert(!ParseComPortNumber(L"LPT1"));
}

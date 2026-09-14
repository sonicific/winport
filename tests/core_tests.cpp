#include "device_rules.h"
#include "device_safety.h"
#include "process_runner.h"

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

    DeviceProtection protection = DeviceProtection::root_enumerated |
                                  DeviceProtection::interrupt_affinity;
    assert(IsProtected(protection));
    assert(HasProtection(protection, DeviceProtection::root_enumerated));
    assert(HasProtection(protection, DeviceProtection::interrupt_affinity));
    assert(!HasProtection(protection, DeviceProtection::software_device));
    assert(DescribeDeviceProtection(DeviceProtection::none) == L"Ghost");
    assert(DescribeDeviceProtection(protection) ==
           L"IRQ affinity configured; ROOT device");

    assert(ParseComPortNumber(L"COM1") == 1u);
    assert(ParseComPortNumber(L"com256") == 256u);
    assert(!ParseComPortNumber(L"COM0"));
    assert(!ParseComPortNumber(L"COM7x"));
    assert(!ParseComPortNumber(L"LPT1"));

    assert(QuoteWindowsArgument(L"") == L"\"\"");
    assert(QuoteWindowsArgument(L"simple") == L"simple");
    assert(QuoteWindowsArgument(L"two words") == L"\"two words\"");
    assert(QuoteWindowsArgument(L"C:\\space path\\") ==
           L"\"C:\\space path\\\\\"");
}

#pragma once

#include <string_view>

namespace sonic79 {

class ComPortManager {
public:
    static bool ReleaseReservationIfUnused(std::wstring_view port_name,
                                           std::wstring_view removed_instance_id);
};

}  // namespace sonic79

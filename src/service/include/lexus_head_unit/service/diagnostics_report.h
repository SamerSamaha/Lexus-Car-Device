#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lexus_head_unit {

// One stored trouble code and its text (DN-030). The text is attached by the source, which
// owns the meaning of its codes; the HMI only shows it.
struct TroubleCode {
    std::string code;
    std::string description;
};

// The result of one diagnostics read (Mode 03 and Mode 09 PID 02, REQ-021). The vehicle
// identification number is private (D-023): it is shown on screen and written nowhere.
struct DiagnosticsReport {
    bool codesRead = false;
    std::vector<TroubleCode> troubleCodes;
    bool identificationRead = false;
    std::string vehicleIdentification;
    std::int64_t timestampMilliseconds = 0;
};

} // namespace lexus_head_unit

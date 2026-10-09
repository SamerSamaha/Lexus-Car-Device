#pragma once

#include <cstdint>
#include <string>

namespace lexus_head_unit {

// Boot time for REQ-013 (LHU-032): on the first frame the hub presents, writes the time since
// the kernel started (CLOCK_BOOTTIME, milliseconds) to a file, once:
//   first_frame_boottime_ms=<n>
// tools/measure/measure_boot_time.py reads it after each cold boot. Time spent in the firmware
// and bootloader before the kernel is not in CLOCK_BOOTTIME; the procedure measures it apart.
class FirstFrameMarker {
public:
    explicit FirstFrameMarker(std::string path);

    // Writes on the first call only; returns true when it wrote. Safe from any one thread.
    bool markFramePresented();
    [[nodiscard]] bool written() const;

    // Milliseconds since the kernel started.
    static std::int64_t bootTimeMilliseconds();

private:
    std::string m_path;
    bool m_written = false;
};

} // namespace lexus_head_unit

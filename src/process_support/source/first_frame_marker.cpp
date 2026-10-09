#include "lexus_head_unit/process_support/first_frame_marker.h"

#include <cstdint>
#include <ctime>
#include <fstream>
#include <ios>
#include <string>
#include <utility>

// glibc declares clock_gettime and CLOCK_BOOTTIME in internal headers; <ctime> is the public one.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr std::int64_t millisecondsPerSecond = 1000;
constexpr std::int64_t nanosecondsPerMillisecond = 1000000;

} // namespace

FirstFrameMarker::FirstFrameMarker(std::string path) : m_path(std::move(path)) {}

bool FirstFrameMarker::markFramePresented() {
    if (m_written) {
        return false;
    }
    m_written = true;
    std::ofstream file(m_path, std::ios::out | std::ios::trunc);
    file << "first_frame_boottime_ms=" << bootTimeMilliseconds() << '\n';
    return file.good();
}

bool FirstFrameMarker::written() const {
    return m_written;
}

std::int64_t FirstFrameMarker::bootTimeMilliseconds() {
    timespec now{};
    if (::clock_gettime(CLOCK_BOOTTIME, &now) != 0) {
        return 0;
    }
    return (static_cast<std::int64_t>(now.tv_sec) * millisecondsPerSecond) +
           (static_cast<std::int64_t>(now.tv_nsec) / nanosecondsPerMillisecond);
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)

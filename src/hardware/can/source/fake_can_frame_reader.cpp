#include "lexus_head_unit/hardware/fake_can_frame_reader.h"

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/can_frame_reader.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace lexus_head_unit {

bool FakeCanFrameReader::open() {
    ++m_openCount;
    m_open = !m_openFails && !m_linkBroken;
    return m_open;
}

CanReadResult FakeCanFrameReader::read(std::int64_t /*timeoutMilliseconds*/) {
    CanReadResult result;
    if (!m_open) {
        result.status = CanReadStatus::Closed;
        return result;
    }
    if (m_linkBroken) {
        result.status = CanReadStatus::Error;
        return result;
    }
    if (m_frames.empty()) {
        result.status = CanReadStatus::Timeout;
        return result;
    }
    result.status = CanReadStatus::Frame;
    result.frame = m_frames.front();
    m_frames.pop_front();
    return result;
}

void FakeCanFrameReader::close() {
    m_open = false;
}

bool FakeCanFrameReader::isOpen() const {
    return m_open;
}

std::string FakeCanFrameReader::lastError() const {
    return m_linkBroken ? "fake link broken" : std::string();
}

void FakeCanFrameReader::queueFrame(const CanFrame& frame) {
    m_frames.push_back(frame);
}

void FakeCanFrameReader::setOpenFails(bool openFails) {
    m_openFails = openFails;
}

void FakeCanFrameReader::breakLink() {
    m_linkBroken = true;
}

void FakeCanFrameReader::restoreLink() {
    m_linkBroken = false;
}

std::size_t FakeCanFrameReader::queuedFrameCount() const {
    return m_frames.size();
}

int FakeCanFrameReader::openCount() const {
    return m_openCount;
}

} // namespace lexus_head_unit

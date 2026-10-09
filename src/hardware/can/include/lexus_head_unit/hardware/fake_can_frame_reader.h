#pragma once

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/can_frame_reader.h"

#include <cstdint>
#include <deque>
#include <string>

namespace lexus_head_unit {

// A CanFrameReader for tests: frames are queued by the test, the link can be broken and
// restored, and opening can be made to fail. Like the real reader it cannot send (REQ-001).
class FakeCanFrameReader final : public CanFrameReader {
public:
    bool open() override;
    CanReadResult read(std::int64_t timeoutMilliseconds) override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
    [[nodiscard]] std::string lastError() const override;

    void queueFrame(const CanFrame& frame);
    void setOpenFails(bool openFails);
    // Reads return Error until restoreLink(); the next open() fails until then too.
    void breakLink();
    void restoreLink();
    [[nodiscard]] std::size_t queuedFrameCount() const;
    [[nodiscard]] int openCount() const;

private:
    std::deque<CanFrame> m_frames;
    bool m_open = false;
    bool m_openFails = false;
    bool m_linkBroken = false;
    int m_openCount = 0;
};

} // namespace lexus_head_unit

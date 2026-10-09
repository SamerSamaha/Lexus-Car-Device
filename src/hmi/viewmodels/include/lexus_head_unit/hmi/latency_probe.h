#pragma once

#include "lexus_head_unit/hmi/value_types.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <string>

namespace lexus_head_unit {

// Sample-to-screen latency for REQ-009 (LHU-032). Each Valid sample that reaches the view
// models is remembered with its arrival time; the first frame presented after it closes it,
// and one CSV row is written:
//   signal,sample_ms,arrival_ms,frame_ms
// sample_ms is the time the source stamped on the sample (when it left the source), frame_ms
// the time the frame was swapped, both on the same monotonic clock, which on Linux is shared
// by every process, so the D-Bus hop is included. The frame time is taken on the render thread
// (markFramePresented) and handed to this object's thread through a queued signal.
class LatencyProbe : public QObject {
    Q_OBJECT

public:
    // Milliseconds on the monotonic clock the source stamps samples with (steady_clock).
    using TimeSource = std::function<std::int64_t()>;

    LatencyProbe(TimeSource now, const std::string& path, QObject* parent = nullptr);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] std::uint64_t rowsWritten() const;
    [[nodiscard]] std::size_t pendingSamples() const;

    // Callable from any thread, typically the render thread right after a swap.
    void markFramePresented();

    static constexpr const char* header = "signal,sample_ms,arrival_ms,frame_ms";

public slots:
    void onSample(lexus_head_unit::SignalSample sample);
    void onFramePresentedAt(qint64 frameMilliseconds);

signals:
    void framePresentedAt(qint64 frameMilliseconds);

private:
    struct Pending {
        SignalSample sample;
        std::int64_t arrivalMilliseconds = 0;
    };

    TimeSource m_now;
    std::ofstream m_file;
    std::deque<Pending> m_pending;
    std::uint64_t m_rowsWritten = 0;
};

} // namespace lexus_head_unit

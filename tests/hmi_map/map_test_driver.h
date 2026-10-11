#pragma once

#include "lexus_head_unit/hmi/map_view_model.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>

#include <cstdint>

namespace lexus_head_unit::testing {

// Plays the vehicle-data service and the screen recorder for the map's QML tests (REQ-030):
// delivers a position as the three phone signals and switches recording on or off.
class MapTestDriver : public QObject {
    Q_OBJECT

public:
    explicit MapTestDriver(QObject* parent = nullptr) : QObject(parent) {}

    void setModel(MapViewModel& model) {
        m_model = &model;
    }

    [[nodiscard]] bool recording() const {
        return m_recording;
    }

    Q_INVOKABLE void deliver(double latitude, double longitude, double accuracy, bool live) {
        const SignalStatus status = live ? SignalStatus::Valid : SignalStatus::Stale;
        m_timestamp += 1000;
        m_model->onSample(sampleOf(SignalId::Latitude, latitude, status));
        m_model->onSample(sampleOf(SignalId::Longitude, longitude, status));
        m_model->onSample(sampleOf(SignalId::PositionAccuracy, accuracy, status));
    }

    Q_INVOKABLE void setRecording(bool recording) {
        m_recording = recording;
        m_model->refresh();
    }

private:
    [[nodiscard]] SignalSample
    sampleOf(SignalId signalId, double value, SignalStatus status) const {
        SignalSample sample;
        sample.signalId = signalId;
        sample.value = value;
        sample.status = status;
        sample.timestampMilliseconds = m_timestamp;
        return sample;
    }

    MapViewModel* m_model = nullptr;
    bool m_recording = false;
    std::int64_t m_timestamp = 0;
};

} // namespace lexus_head_unit::testing

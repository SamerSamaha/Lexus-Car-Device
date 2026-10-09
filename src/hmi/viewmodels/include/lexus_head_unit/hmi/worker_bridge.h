#pragma once

#include "lexus_head_unit/hmi/value_types.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>

namespace lexus_head_unit {

// The thread hop. The worker thread emits these signals; the view model's slots are connected
// with Qt::QueuedConnection, so Qt copies the values and runs the slots on the UI thread.
class WorkerBridge : public QObject {
    Q_OBJECT

public:
    explicit WorkerBridge(QObject* parent = nullptr);

    // Registers the two value types for queued delivery; safe to call more than once.
    static void registerTypes();

    void publishSample(const SignalSample& sample);
    void publishConnectionChange(const ConnectionTransition& transition);

signals:
    void sampleArrived(lexus_head_unit::SignalSample sample);
    void connectionChanged(lexus_head_unit::ConnectionTransition transition);
};

} // namespace lexus_head_unit

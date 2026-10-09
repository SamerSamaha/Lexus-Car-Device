#include "lexus_head_unit/hmi/worker_bridge.h"

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QMetaType>
#include <QObject>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

WorkerBridge::WorkerBridge(QObject* parent) : QObject(parent) {
    registerTypes();
}

void WorkerBridge::registerTypes() {
    qRegisterMetaType<lexus_head_unit::SignalSample>("lexus_head_unit::SignalSample");
    qRegisterMetaType<lexus_head_unit::ConnectionTransition>(
        "lexus_head_unit::ConnectionTransition");
    qRegisterMetaType<lexus_head_unit::DiagnosticsReport>("lexus_head_unit::DiagnosticsReport");
}

void WorkerBridge::publishSample(const SignalSample& sample) {
    emit sampleArrived(sample);
}

void WorkerBridge::publishDiagnostics(const DiagnosticsReport& report) {
    emit diagnosticsArrived(report);
}

void WorkerBridge::publishConnectionChange(const ConnectionTransition& transition) {
    emit connectionChanged(transition);
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)

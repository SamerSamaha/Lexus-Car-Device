#include "fake_vehicle_data_view_model.h"

#include "lexus_head_unit/hmi/vehicle_data_view_model.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QString>

namespace lexus_head_unit::testing {

namespace {

SignalSample sampleFor(int signalIndex, double value, SignalStatus status) {
    SignalSample sample;
    sample.signalId = static_cast<SignalId>(signalIndex);
    sample.value = value;
    sample.unit = definitionOf(sample.signalId).unit;
    sample.timestampMilliseconds = 1;
    sample.status = status;
    return sample;
}

ConnectionState stateNamed(const QString& name) {
    for (const ConnectionState state : allConnectionStates) {
        if (name == QString::fromUtf8(toString(state).data())) {
            return state;
        }
    }
    return ConnectionState::Disconnected;
}

ConnectionTrigger triggerNamed(const QString& name) {
    for (const ConnectionTrigger trigger : allConnectionTriggers) {
        if (name == QString::fromUtf8(toString(trigger).data())) {
            return trigger;
        }
    }
    return ConnectionTrigger::StartRequested;
}

} // namespace

FakeVehicleDataViewModel::FakeVehicleDataViewModel(QObject* parent)
    : VehicleDataViewModel(parent) {}

void FakeVehicleDataViewModel::simulateValid(int signalIndex, double value) {
    onSample(sampleFor(signalIndex, value, SignalStatus::Valid));
}

void FakeVehicleDataViewModel::simulateStale(int signalIndex, double value) {
    onSample(sampleFor(signalIndex, value, SignalStatus::Stale));
}

void FakeVehicleDataViewModel::simulateNeverReceived(int signalIndex) {
    onSample(sampleFor(signalIndex, 0.0, SignalStatus::NeverReceived));
}

void FakeVehicleDataViewModel::simulateConnection(const QString& stateName,
                                                  const QString& triggerName) {
    ConnectionTransition transition;
    transition.to = stateNamed(stateName);
    transition.trigger = triggerNamed(triggerName);
    onConnectionChanged(transition);
}

void FakeVehicleDataViewModel::simulateLinkDetail(const QString& detailName) {
    for (const LinkDetail detail : allLinkDetails) {
        if (detailName == QString::fromUtf8(toString(detail).data())) {
            onLinkDetailChanged(detail);
            return;
        }
    }
}

} // namespace lexus_head_unit::testing

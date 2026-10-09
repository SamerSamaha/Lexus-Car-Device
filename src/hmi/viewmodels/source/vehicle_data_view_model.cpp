#include "lexus_head_unit/hmi/vehicle_data_view_model.h"

#include "lexus_head_unit/hmi/connection_status_model.h"
#include "lexus_head_unit/hmi/signal_tile_model.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QList>
#include <QObject>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

VehicleDataViewModel::VehicleDataViewModel(QObject* parent)
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    : QObject(parent), m_connection(new ConnectionStatusModel(this)) {
    for (const SignalId signalId : allSignalIds) {
        // Qt parent ownership: the tile is deleted with this object.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        m_tiles.at(indexOf(signalId)) = new SignalTileModel(signalId, this);
    }
}

QList<QObject*> VehicleDataViewModel::tiles() const {
    QList<QObject*> list;
    for (const SignalId signalId : gridSignalIds) {
        list.append(tile(signalId));
    }
    return list;
}

QList<QObject*> VehicleDataViewModel::tripTiles() const {
    QList<QObject*> list;
    for (const SignalId signalId : derivedSignalIds) {
        list.append(tile(signalId));
    }
    return list;
}

SignalTileModel* VehicleDataViewModel::tile(SignalId signalId) const {
    if (!isKnownSignal(signalId)) {
        return nullptr;
    }
    return m_tiles.at(indexOf(signalId));
}

SignalTileModel* VehicleDataViewModel::vehicleSpeed() const {
    return tile(SignalId::VehicleSpeed);
}

SignalTileModel* VehicleDataViewModel::engineRpm() const {
    return tile(SignalId::EngineRpm);
}

ConnectionStatusModel* VehicleDataViewModel::connection() const {
    return m_connection;
}

// Not const: it is a slot that changes the tile it owns, even though the pointer array is
// untouched.
// NOLINTNEXTLINE(readability-make-member-function-const)
void VehicleDataViewModel::onSample(lexus_head_unit::SignalSample sample) {
    SignalTileModel* target = tile(sample.signalId);
    if (target != nullptr) {
        target->applySample(sample);
    }
}

void VehicleDataViewModel::onConnectionChanged(lexus_head_unit::ConnectionTransition transition) {
    m_connection->applyTransition(transition);
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)

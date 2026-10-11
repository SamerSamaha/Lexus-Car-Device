#pragma once

#include "lexus_head_unit/hmi/connection_status_model.h"
#include "lexus_head_unit/hmi/display_units.h"
#include "lexus_head_unit/hmi/signal_tile_model.h"
#include "lexus_head_unit/hmi/value_types.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QList>
#include <QObject>

#include <array>

namespace lexus_head_unit {

// The one object QML binds to. Its slots are the targets of the queued bridge signals.
class VehicleDataViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QObject*> tiles READ tiles CONSTANT)
    Q_PROPERTY(QList<QObject*> tripTiles READ tripTiles CONSTANT)
    Q_PROPERTY(lexus_head_unit::SignalTileModel* vehicleSpeed READ vehicleSpeed CONSTANT)
    Q_PROPERTY(lexus_head_unit::SignalTileModel* engineRpm READ engineRpm CONSTANT)
    Q_PROPERTY(lexus_head_unit::ConnectionStatusModel* connection READ connection CONSTANT)
    Q_PROPERTY(bool usUnits READ usUnits CONSTANT)

public:
    explicit VehicleDataViewModel(QObject* parent = nullptr);
    // Every tile in these display units (DN-050); the samples stay metric.
    explicit VehicleDataViewModel(DisplayUnits units, QObject* parent = nullptr);

    // The eight grid signals of REQ-004.
    [[nodiscard]] QList<QObject*> tiles() const;
    // The eight derived signals of REQ-022, for the trip screen.
    [[nodiscard]] QList<QObject*> tripTiles() const;
    [[nodiscard]] SignalTileModel* tile(SignalId signalId) const;
    [[nodiscard]] SignalTileModel* vehicleSpeed() const;
    [[nodiscard]] SignalTileModel* engineRpm() const;
    [[nodiscard]] ConnectionStatusModel* connection() const;
    [[nodiscard]] DisplayUnits displayUnits() const;
    [[nodiscard]] bool usUnits() const;

public slots:
    void onSample(lexus_head_unit::SignalSample sample);
    void onConnectionChanged(lexus_head_unit::ConnectionTransition transition);
    void onLinkDetailChanged(lexus_head_unit::LinkDetail detail);

private:
    std::array<SignalTileModel*, signalCount> m_tiles{};
    ConnectionStatusModel* m_connection;
    DisplayUnits m_units;
};

} // namespace lexus_head_unit

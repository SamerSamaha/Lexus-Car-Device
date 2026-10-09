#pragma once

#include "lexus_head_unit/hmi/vehicle_data_view_model.h"

#include <QObject>
#include <QString>

namespace lexus_head_unit::testing {

// The product view model with test-only entry points that QML tests can call.
class FakeVehicleDataViewModel : public VehicleDataViewModel {
    Q_OBJECT

public:
    explicit FakeVehicleDataViewModel(QObject* parent = nullptr);

    Q_INVOKABLE void simulateValid(int signalIndex, double value);
    Q_INVOKABLE void simulateStale(int signalIndex, double value);
    Q_INVOKABLE void simulateNeverReceived(int signalIndex);
    Q_INVOKABLE void simulateConnection(const QString& stateName, const QString& triggerName);
};

} // namespace lexus_head_unit::testing

// Verifies: REQ-012, REQ-007, REQ-011

#include "lexus_head_unit/hmi/connection_status_model.h"
#include "lexus_head_unit/hmi/signal_tile_model.h"
#include "lexus_head_unit/hmi/vehicle_data_view_model.h"
#include "lexus_head_unit/hmi/worker_bridge.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QList>
#include <QObject>
#include <QSignalSpy>
#include <QString>

#include <cstddef>
#include <thread>

// Qt declares its macros and enumerations in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionStatusModel;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalTileModel;
using lexus_head_unit::Unit;
using lexus_head_unit::VehicleDataViewModel;
using lexus_head_unit::WorkerBridge;

SignalSample sample(SignalId signalId, double value, Unit unit, SignalStatus status) {
    SignalSample result;
    result.signalId = signalId;
    result.value = value;
    result.unit = unit;
    result.timestampMilliseconds = 1;
    result.status = status;
    return result;
}

TEST(SignalTileModelTest, StartsNeverReceivedWithTheDefinitionNameAndUnit) {
    const SignalTileModel tile(SignalId::CoolantTemperature);
    EXPECT_EQ(tile.name(), QStringLiteral("Coolant temperature"));
    EXPECT_EQ(tile.unitText(), QStringLiteral("degC"));
    EXPECT_EQ(tile.valueText(), QStringLiteral("--"));
    EXPECT_EQ(tile.statusText(), QStringLiteral("NeverReceived"));
    EXPECT_TRUE(tile.isNeverReceived());
    EXPECT_FALSE(tile.isValid());
    EXPECT_FALSE(tile.isStale());
}

TEST(SignalTileModelTest, FormatsIntegersForMostUnitsAndOneDecimalForVolts) {
    EXPECT_EQ(SignalTileModel::formatValue(63.4, Unit::KilometresPerHour), QStringLiteral("63"));
    EXPECT_EQ(SignalTileModel::formatValue(1726.0, Unit::RevolutionsPerMinute),
              QStringLiteral("1726"));
    EXPECT_EQ(SignalTileModel::formatValue(-10.0, Unit::DegreesCelsius), QStringLiteral("-10"));
    EXPECT_EQ(SignalTileModel::formatValue(50.19, Unit::Percent), QStringLiteral("50"));
    EXPECT_EQ(SignalTileModel::formatValue(14.1, Unit::Volts), QStringLiteral("14.1"));
}

TEST(SignalTileModelTest, ValidStaleAndNeverReceivedSamplesSetTextAndFlagsAndNotify) {
    SignalTileModel tile(SignalId::VehicleSpeed);
    const QSignalSpy spy(&tile, &SignalTileModel::changed);

    tile.applySample(
        sample(SignalId::VehicleSpeed, 63.0, Unit::KilometresPerHour, SignalStatus::Valid));
    EXPECT_EQ(tile.valueText(), QStringLiteral("63"));
    EXPECT_TRUE(tile.isValid());
    EXPECT_EQ(tile.statusText(), QStringLiteral("Valid"));

    tile.applySample(
        sample(SignalId::VehicleSpeed, 63.0, Unit::KilometresPerHour, SignalStatus::Stale));
    EXPECT_EQ(tile.valueText(), QStringLiteral("63"));
    EXPECT_TRUE(tile.isStale());
    EXPECT_FALSE(tile.isValid());

    tile.applySample(
        sample(SignalId::VehicleSpeed, 0.0, Unit::KilometresPerHour, SignalStatus::NeverReceived));
    EXPECT_EQ(tile.valueText(), QStringLiteral("--"));
    EXPECT_TRUE(tile.isNeverReceived());
    EXPECT_EQ(spy.count(), 3);
}

TEST(SignalTileModelTest, SampleForAnotherSignalIsIgnored) {
    SignalTileModel tile(SignalId::VehicleSpeed);
    const QSignalSpy spy(&tile, &SignalTileModel::changed);
    tile.applySample(
        sample(SignalId::EngineRpm, 800.0, Unit::RevolutionsPerMinute, SignalStatus::Valid));
    EXPECT_EQ(tile.valueText(), QStringLiteral("--"));
    EXPECT_EQ(spy.count(), 0);
}

TEST(ConnectionStatusModelTest, StartsDisconnectedAndFollowsTransitions) {
    ConnectionStatusModel model;
    EXPECT_EQ(model.stateText(), QStringLiteral("Disconnected"));
    EXPECT_FALSE(model.isConnected());
    EXPECT_FALSE(model.isError());
    const QSignalSpy spy(&model, &ConnectionStatusModel::changed);

    ConnectionTransition transition;
    transition.from = ConnectionState::Connecting;
    transition.trigger = ConnectionTrigger::HandshakeSucceeded;
    transition.to = ConnectionState::Connected;
    model.applyTransition(transition);
    EXPECT_EQ(model.stateText(), QStringLiteral("Connected"));
    EXPECT_EQ(model.lastCauseText(), QStringLiteral("HandshakeSucceeded"));
    EXPECT_TRUE(model.isConnected());

    transition.from = ConnectionState::Connected;
    transition.trigger = ConnectionTrigger::LinkLost;
    transition.to = ConnectionState::Error;
    model.applyTransition(transition);
    EXPECT_EQ(model.stateText(), QStringLiteral("Error"));
    EXPECT_TRUE(model.isError());
    EXPECT_FALSE(model.isConnected());
    EXPECT_EQ(model.transitionCount(), 2U);
    EXPECT_EQ(spy.count(), 2);
}

// True when the tile for the id exists, carries the id, and sits at the id's index in tiles().
bool tileIsInPlace(const VehicleDataViewModel& viewModel, SignalId signalId) {
    const SignalTileModel* tile = viewModel.tile(signalId);
    if (tile == nullptr || tile->signalId() != signalId) {
        return false;
    }
    const QList<QObject*> tiles = viewModel.tiles();
    const int position = static_cast<int>(lexus_head_unit::indexOf(signalId));
    return position < tiles.size() && tiles.at(position) == tile;
}

TEST(VehicleDataViewModelTest, HasOneTilePerSignalInEnumerationOrder) {
    const VehicleDataViewModel viewModel;
    ASSERT_EQ(viewModel.tiles().size(), 8);
    std::size_t inPlace = 0;
    for (const SignalId signalId : lexus_head_unit::gridSignalIds) {
        inPlace += tileIsInPlace(viewModel, signalId) ? 1U : 0U;
    }
    EXPECT_EQ(inPlace, 8U);
    EXPECT_EQ(viewModel.vehicleSpeed()->name(), QStringLiteral("Vehicle speed"));
    EXPECT_EQ(viewModel.engineRpm()->unitText(), QStringLiteral("rpm"));
    EXPECT_NE(viewModel.connection(), nullptr);
}

TEST(VehicleDataViewModelTest, TripTilesAreTheEightDerivedSignalsInOrder) {
    const VehicleDataViewModel viewModel;
    const QList<QObject*> tripTiles = viewModel.tripTiles();
    ASSERT_EQ(tripTiles.size(), 8);
    std::size_t inOrder = 0;
    for (std::size_t index = 0; index < lexus_head_unit::derivedSignalCount; ++index) {
        const SignalId signalId = lexus_head_unit::derivedSignalIds.at(index);
        inOrder += tripTiles.at(static_cast<int>(index)) == viewModel.tile(signalId) ? 1U : 0U;
    }
    EXPECT_EQ(inOrder, 8U);
}

TEST(VehicleDataViewModelTest, SamplesAndTransitionsFromAnotherThreadArriveThroughTheBridge) {
    WorkerBridge bridge;
    const VehicleDataViewModel viewModel;
    QObject::connect(&bridge,
                     &WorkerBridge::sampleArrived,
                     &viewModel,
                     &VehicleDataViewModel::onSample,
                     Qt::QueuedConnection);
    QObject::connect(&bridge,
                     &WorkerBridge::connectionChanged,
                     &viewModel,
                     &VehicleDataViewModel::onConnectionChanged,
                     Qt::QueuedConnection);

    std::thread worker([&bridge]() {
        bridge.publishSample(
            sample(SignalId::EngineRpm, 850.0, Unit::RevolutionsPerMinute, SignalStatus::Valid));
        ConnectionTransition transition;
        transition.from = ConnectionState::Connecting;
        transition.trigger = ConnectionTrigger::HandshakeSucceeded;
        transition.to = ConnectionState::Connected;
        bridge.publishConnectionChange(transition);
    });
    worker.join();

    // Nothing has been delivered yet: the slots run on this thread's event loop.
    EXPECT_EQ(viewModel.engineRpm()->valueText(), QStringLiteral("--"));
    QCoreApplication::processEvents();
    EXPECT_EQ(viewModel.engineRpm()->valueText(), QStringLiteral("850"));
    EXPECT_TRUE(viewModel.engineRpm()->isValid());
    EXPECT_TRUE(viewModel.connection()->isConnected());
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    ::testing::InitGoogleTest(&argumentCount, argumentValues);
    return RUN_ALL_TESTS();
}
// NOLINTEND(misc-include-cleaner)

// Verifies: REQ-030

#include "lexus_head_unit/hmi/map_view_model.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QSignalSpy>
#include <QString>

#include <gtest/gtest.h>

#include <optional>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::distanceMetres;
using lexus_head_unit::HomeArea;
using lexus_head_unit::isPositionMasked;
using lexus_head_unit::MapViewModel;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;

// An invented home in the middle of Lake Michigan, so no real address is in the repository.
constexpr double homeLatitude = 43.5;
constexpr double homeLongitude = -87.0;
const HomeArea home{homeLatitude, homeLongitude, 500.0};
// 0.001 degree of latitude is about 111 m.
constexpr double metresPerMilliDegreeOfLatitude = 111.2;

SignalSample sampleOf(SignalId signalId, double value, SignalStatus status = SignalStatus::Valid) {
    SignalSample sample;
    sample.signalId = signalId;
    sample.value = value;
    sample.status = status;
    sample.timestampMilliseconds = 1000;
    return sample;
}

void deliver(MapViewModel& model,
             double latitude,
             double longitude,
             double accuracy,
             SignalStatus status = SignalStatus::Valid) {
    model.onSample(sampleOf(SignalId::Latitude, latitude, status));
    model.onSample(sampleOf(SignalId::Longitude, longitude, status));
    model.onSample(sampleOf(SignalId::PositionAccuracy, accuracy, status));
}

TEST(MapDistanceTest, HaversineMatchesKnownDistances) {
    EXPECT_NEAR(
        distanceMetres(homeLatitude, homeLongitude, homeLatitude, homeLongitude), 0.0, 1e-9);
    EXPECT_NEAR(distanceMetres(homeLatitude, homeLongitude, homeLatitude + 0.001, homeLongitude),
                metresPerMilliDegreeOfLatitude,
                0.5);
    // A quarter of the equator: 10,007.5 km on the 6,371 km sphere.
    EXPECT_NEAR(distanceMetres(0.0, 0.0, 0.0, 90.0), 10007543.4, 1.0);
}

TEST(MapMaskTest, NearHomeIsHiddenOnlyWhileRecordingUnlessAlways) {
    const double inside = homeLatitude + 0.004;  // about 445 m
    const double outside = homeLatitude + 0.005; // about 556 m
    EXPECT_TRUE(isPositionMasked(home, inside, homeLongitude, true, false));
    EXPECT_FALSE(isPositionMasked(home, outside, homeLongitude, true, false));
    EXPECT_FALSE(isPositionMasked(home, inside, homeLongitude, false, false));
    EXPECT_TRUE(isPositionMasked(home, inside, homeLongitude, false, true));
    EXPECT_FALSE(isPositionMasked(std::nullopt, homeLatitude, homeLongitude, true, true))
        << "no home area set: nothing to hide";
}

TEST(MapViewModelTest, BeforeAnyPositionItWaitsForThePhone) {
    const MapViewModel model(std::nullopt, false, []() {
        return false;
    });
    EXPECT_FALSE(model.hasPosition());
    EXPECT_EQ(model.statusText(),
              QStringLiteral("Waiting for the phone: open the phone link in Safari"));
}

TEST(MapViewModelTest, APositionIsTakenOverOnlyWhenItsAccuracyArrives) {
    MapViewModel model(std::nullopt, false, []() {
        return false;
    });
    model.onSample(sampleOf(SignalId::Latitude, 42.0));
    model.onSample(sampleOf(SignalId::Longitude, -83.0));
    EXPECT_FALSE(model.hasPosition()) << "not before the accuracy";
    model.onSample(sampleOf(SignalId::PositionAccuracy, 6.4));
    EXPECT_TRUE(model.hasPosition());
    EXPECT_TRUE(model.positionLive());
    EXPECT_DOUBLE_EQ(model.latitude(), 42.0);
    EXPECT_DOUBLE_EQ(model.longitude(), -83.0);
    EXPECT_EQ(model.statusText(), QStringLiteral("Phone position, 6 m"));
}

TEST(MapViewModelTest, HalfAPositionAndOtherSignalsChangeNothing) {
    MapViewModel model(std::nullopt, false, []() {
        return false;
    });
    deliver(model, 42.0, -83.0, 5.0);
    model.onSample(sampleOf(SignalId::Latitude, 43.0));
    EXPECT_DOUBLE_EQ(model.latitude(), 42.0) << "half a position never shows";
    model.onSample(sampleOf(SignalId::EngineRpm, 900.0));
    EXPECT_DOUBLE_EQ(model.latitude(), 42.0);
}

TEST(MapViewModelTest, AStalePositionStaysButSaysItIsLost) {
    MapViewModel model(std::nullopt, false, []() {
        return false;
    });
    deliver(model, 42.0, -83.0, 5.0);
    deliver(model, 42.0, -83.0, 5.0, SignalStatus::Stale);
    EXPECT_TRUE(model.hasPosition());
    EXPECT_FALSE(model.positionLive());
    EXPECT_EQ(model.statusText(),
              QStringLiteral("Phone position lost: keep the page open on the phone"));
}

TEST(MapViewModelTest, StartingARecordingNearHomeHidesTheCoordinates) {
    bool recording = false;
    MapViewModel model(home, false, [&recording]() {
        return recording;
    });
    deliver(model, homeLatitude + 0.001, homeLongitude, 5.0);
    EXPECT_FALSE(model.masked());
    const QSignalSpy changes(&model, &MapViewModel::changed);
    recording = true;
    model.refresh();
    EXPECT_TRUE(model.masked());
    EXPECT_EQ(changes.count(), 1);
    EXPECT_DOUBLE_EQ(model.latitude(), 0.0) << "the coordinates never reach the screen";
    EXPECT_DOUBLE_EQ(model.longitude(), 0.0);
    EXPECT_EQ(model.statusText(), QStringLiteral("Position hidden near home"));
}

TEST(MapViewModelTest, AwayFromHomeOrNotRecordingTheCoordinatesAreShown) {
    bool recording = true;
    MapViewModel model(home, false, [&recording]() {
        return recording;
    });
    deliver(model, homeLatitude + 0.02, homeLongitude, 5.0);
    EXPECT_FALSE(model.masked()) << "2 km away is shown while recording";
    recording = false;
    deliver(model, homeLatitude, homeLongitude, 5.0);
    EXPECT_FALSE(model.masked()) << "not recording: shown";
}

} // namespace
// NOLINTEND(misc-include-cleaner)

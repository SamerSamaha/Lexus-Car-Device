// Verifies: REQ-020

#include "fake_power_status_reader.h"

#include "lexus_head_unit/hub/power_status_model.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::PowerStatusModel;
using lexus_head_unit::ShutdownController;
using lexus_head_unit::testing::FakePowerStatusReader;

auto flagsOf(const PowerStatusModel& model) {
    return std::make_tuple(model.underVoltage(),
                           model.frequencyCapped(),
                           model.throttled(),
                           model.softTemperatureLimit());
}

TEST(PowerStatusModelTest, EachFlagIsSetAndClearedFromTheReading) {
    FakePowerStatusReader reader;
    PowerStatusModel model(reader);
    const std::vector<std::pair<QString, std::tuple<bool, bool, bool, bool>>> cases = {
        {QStringLiteral("throttled=0x1\n"), {true, false, false, false}},
        {QStringLiteral("throttled=0x2\n"), {false, true, false, false}},
        {QStringLiteral("throttled=0x4\n"), {false, false, true, false}},
        {QStringLiteral("throttled=0x8\n"), {false, false, false, true}},
        {QStringLiteral("throttled=0x0\n"), {false, false, false, false}},
    };
    std::vector<std::tuple<bool, bool, bool, bool>> observed;
    std::vector<std::tuple<bool, bool, bool, bool>> expected;
    for (const auto& [output, flags] : cases) {
        reader.setOutput(output);
        model.poll();
        observed.push_back(flagsOf(model));
        expected.push_back(flags);
    }
    EXPECT_EQ(observed, expected);
    EXPECT_TRUE(model.available());
    EXPECT_EQ(model.flagsText(), QStringLiteral("Power OK"));
}

TEST(PowerStatusModelTest, TextSinceBootAndUnavailable) {
    FakePowerStatusReader reader;
    PowerStatusModel model(reader);
    const QSignalSpy changes(&model, &PowerStatusModel::changed);
    reader.setOutput(QStringLiteral("throttled=0x50005\n"));
    model.poll();
    EXPECT_EQ(model.flagsText(), QStringLiteral("Under-voltage, Throttled"));
    EXPECT_TRUE(model.occurredSinceBoot());
    model.poll();
    EXPECT_EQ(changes.count(), 1);
    reader.setOutput(QString(), false);
    model.poll();
    EXPECT_FALSE(model.available());
    EXPECT_EQ(model.flagsText(), QStringLiteral("Power status unavailable"));
    EXPECT_EQ(flagsOf(model), std::make_tuple(false, false, false, false));
    reader.setOutput(QStringLiteral("VCHI initialization failed"));
    model.poll();
    EXPECT_FALSE(model.available());
}

TEST(PowerStatusModelTest, APollWhileAReadingRunsIsSkipped) {
    FakePowerStatusReader reader;
    reader.holdReplies(true);
    PowerStatusModel model(reader);
    model.poll();
    model.poll();
    model.poll();
    EXPECT_EQ(reader.requests(), 1);
    EXPECT_EQ(model.skippedPolls(), 2U);
    reader.setOutput(QStringLiteral("throttled=0x4\n"));
    reader.answer();
    EXPECT_TRUE(model.throttled());
}

// "armed" or "idle", the result text and the run count, so one press compares in one line.
QString stateOf(const ShutdownController& controller, std::size_t commandsStarted) {
    return QStringLiteral("%1|%2|%3|%4")
        .arg(controller.armed() ? QStringLiteral("armed") : QStringLiteral("idle"))
        .arg(controller.lastResultText())
        .arg(controller.executions())
        .arg(commandsStarted);
}

TEST(ShutdownControllerTest, FirstPressArmsSecondRunsTheCommandOnce) {
    std::vector<QStringList> started;
    ShutdownController controller({QStringLiteral("systemctl"), QStringLiteral("poweroff")},
                                  [&started](const QString& program, const QStringList& arguments) {
                                      started.push_back(QStringList{program} + arguments);
                                      return true;
                                  });
    controller.press();
    EXPECT_EQ(stateOf(controller, started.size()),
              QStringLiteral("armed|Tap again to shut down|0|0"));
    controller.press();
    EXPECT_EQ(stateOf(controller, started.size()), QStringLiteral("idle|Shutting down|1|1"));
    EXPECT_EQ(started.front(),
              (QStringList{QStringLiteral("systemctl"), QStringLiteral("poweroff")}));
}

TEST(ShutdownControllerTest, ArmingExpiresAndFailureIsReported) {
    int calls = 0;
    ShutdownController controller(
        {QStringLiteral("false")},
        [&calls](const QString& /*program*/, const QStringList& /*arguments*/) {
            ++calls;
            return false;
        },
        50);
    controller.press();
    QElapsedTimer timer;
    timer.start();
    while (controller.armed() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    EXPECT_FALSE(controller.armed());
    EXPECT_EQ(calls, 0);
    controller.press();
    controller.press();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(controller.lastResultText(), QStringLiteral("Shutdown command could not start"));
    ShutdownController empty({}, ShutdownController::detachedProcessExecutor());
    empty.press();
    empty.press();
    EXPECT_EQ(empty.lastResultText(), QStringLiteral("No shutdown command configured"));
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    ::testing::InitGoogleTest(&argumentCount, argumentValues);
    return RUN_ALL_TESTS();
}
// NOLINTEND(misc-include-cleaner)

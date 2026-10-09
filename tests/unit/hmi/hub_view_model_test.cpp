// Verifies: REQ-016

#include "fake_process_launcher.h"

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/app_process_manager.h"
#include "lexus_head_unit/hub/hub_control_server.h"
#include "lexus_head_unit/hub/hub_view_model.h"
#include "lexus_head_unit/hub/process_launcher.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <QCoreApplication>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <gtest/gtest.h>

#include <vector>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::AppEntry;
using lexus_head_unit::AppListModel;
using lexus_head_unit::AppProcessManager;
using lexus_head_unit::HubControlServer;
using lexus_head_unit::HubViewModel;
using lexus_head_unit::ManualClock;
using lexus_head_unit::ProcessExit;
using lexus_head_unit::ProcessExitKind;
using lexus_head_unit::testing::FakeProcessLauncher;

std::vector<AppEntry> twoApps() {
    AppEntry music;
    music.id = "music";
    music.name = "Music";
    music.icon = "/icons/music.png";
    music.arguments = {"music-player"};
    AppEntry vehicleData;
    vehicleData.id = "vehicle_data";
    vehicleData.name = "Vehicle data";
    vehicleData.arguments = {"lexus-head-unit"};
    return {music, vehicleData};
}

class HubViewModelTest : public ::testing::Test {
protected:
    ManualClock m_clock{0};
    FakeProcessLauncher m_launcher;
    AppProcessManager m_manager{m_launcher, m_clock};
    HubViewModel m_hub{m_manager, twoApps(), QStringList()};
};

TEST_F(HubViewModelTest, ListModelRowsAndRoles) {
    AppListModel* apps = m_hub.apps();
    ASSERT_EQ(apps->rowCount(), 2);
    const QModelIndex first = apps->index(0, 0);
    EXPECT_EQ(apps->data(first, AppListModel::AppIdRole).toString(), QStringLiteral("music"));
    EXPECT_EQ(apps->data(first, AppListModel::NameRole).toString(), QStringLiteral("Music"));
    EXPECT_EQ(apps->data(first, AppListModel::IconRole).toString(),
              QStringLiteral("/icons/music.png"));
    EXPECT_FALSE(apps->data(apps->index(5, 0), AppListModel::NameRole).isValid());
    EXPECT_EQ(apps->roleNames().value(AppListModel::NameRole), "name");
    EXPECT_EQ(apps->entryAt(-1), nullptr);
    EXPECT_EQ(apps->entryAt(2), nullptr);
}

TEST_F(HubViewModelTest, LaunchSetsRunningStateAndNotifiesOnce) {
    const QSignalSpy changes(&m_hub, &HubViewModel::stateChanged);
    EXPECT_FALSE(m_hub.launch(7));
    EXPECT_EQ(changes.count(), 0);
    EXPECT_TRUE(m_hub.launch(1));
    EXPECT_TRUE(m_hub.appRunning());
    EXPECT_EQ(m_hub.foregroundAppName(), QStringLiteral("Vehicle data"));
    EXPECT_EQ(changes.count(), 1);
    m_hub.poll();
    EXPECT_EQ(changes.count(), 1);
    EXPECT_FALSE(m_hub.launch(0));
}

TEST_F(HubViewModelTest, ExitOfTheAppReturnsToIdleWithTheExitText) {
    ASSERT_TRUE(m_hub.launch(0));
    const QSignalSpy changes(&m_hub, &HubViewModel::stateChanged);
    m_launcher.finish(m_launcher.lastProcessId(), ProcessExit{ProcessExitKind::Exited, 4});
    m_hub.poll();
    EXPECT_FALSE(m_hub.appRunning());
    EXPECT_EQ(m_hub.foregroundAppName(), QString());
    EXPECT_EQ(m_hub.lastExitText(), QStringLiteral("Music failed (exit code 4)"));
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(HubViewModelTest, ControlCommandsWhileIdle) {
    HubControlServer server(m_hub);
    EXPECT_EQ(server.handleCommand(QStringLiteral("")), QStringLiteral("error empty command"));
    EXPECT_EQ(server.handleCommand(QStringLiteral("return")),
              QStringLiteral("return nothing-running"));
    EXPECT_EQ(server.handleCommand(QStringLiteral("launch nothing")),
              QStringLiteral("launch unknown app"));
    EXPECT_EQ(server.handleCommand(QStringLiteral("launch")),
              QStringLiteral("error unknown command"));
}

TEST_F(HubViewModelTest, ControlCommandsWithAnAppRunning) {
    HubControlServer server(m_hub);
    EXPECT_EQ(server.handleCommand(QStringLiteral("launch music")),
              QStringLiteral("launch started"));
    const QString runningStatus = server.handleCommand(QStringLiteral("status"));
    EXPECT_TRUE(runningStatus.startsWith(QStringLiteral("state running app music hub_pid ")))
        << runningStatus.toStdString();
    EXPECT_TRUE(runningStatus.contains(QStringLiteral(" app_pid 1000 window hidden ")))
        << runningStatus.toStdString();
    EXPECT_EQ(server.handleCommand(QStringLiteral("launch vehicle_data")),
              QStringLiteral("launch already-running"));
    EXPECT_EQ(server.handleCommand(QStringLiteral("return")), QStringLiteral("return stopping"));
}

TEST_F(HubViewModelTest, StatusAfterAStoppedApp) {
    HubControlServer server(m_hub);
    ASSERT_EQ(server.handleCommand(QStringLiteral("launch music")),
              QStringLiteral("launch started"));
    ASSERT_EQ(server.handleCommand(QStringLiteral("return")), QStringLiteral("return stopping"));
    m_hub.poll();
    server.setWindowVisibilityProvider([]() {
        return true;
    });
    const QString idleStatus = server.handleCommand(QStringLiteral("  status  "));
    EXPECT_TRUE(idleStatus.startsWith(QStringLiteral("state idle app - hub_pid ")))
        << idleStatus.toStdString();
    EXPECT_TRUE(idleStatus.endsWith(
        QStringLiteral(" app_pid - window visible launches 1 exits 1 restarts 0 kills 0")))
        << idleStatus.toStdString();
    EXPECT_EQ(m_hub.lastExitText(), QStringLiteral("Music stopped"));
}

TEST(HubViewModelErrorsTest, RegistryErrorsAreJoinedForTheStatusStrip) {
    const ManualClock clock{0};
    FakeProcessLauncher launcher;
    AppProcessManager manager(launcher, clock);
    const HubViewModel hub(manager, {}, {QStringLiteral("first"), QStringLiteral("second")});
    EXPECT_EQ(hub.registryErrorText(), QStringLiteral("first; second"));
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    ::testing::InitGoogleTest(&argumentCount, argumentValues);
    return RUN_ALL_TESTS();
}
// NOLINTEND(misc-include-cleaner)

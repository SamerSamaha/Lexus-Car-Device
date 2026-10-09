// Verifies: REQ-024, REQ-020

#include "lexus_head_unit/hub/car_status_models.h"
#include "lexus_head_unit/hub/ignition_off_shutdown_policy.h"
#include "lexus_head_unit/hub/power_status_model.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::IgnitionOffShutdownModel;
using lexus_head_unit::IgnitionOffShutdownSettings;
using lexus_head_unit::LinkDetail;
using lexus_head_unit::ManualClock;
using lexus_head_unit::NetworkAddressModel;
using lexus_head_unit::ShutdownController;

class IgnitionOffShutdownModelTest : public ::testing::Test {
protected:
    ManualClock m_clock{0};
    int m_executions = 0;
    QStringList m_lastCommand;
    ShutdownController m_shutdown{
        QStringList{QStringLiteral("systemctl"), QStringLiteral("poweroff")},
        [this](const QString& program, const QStringList& arguments) {
            ++m_executions;
            m_lastCommand = QStringList{program} + arguments;
            return true;
        }};
    IgnitionOffShutdownModel m_model{IgnitionOffShutdownSettings{300000, 60000},
                                     [this]() {
                                         return m_clock.nowMilliseconds();
                                     },
                                     m_shutdown};
};

TEST_F(IgnitionOffShutdownModelTest, CountdownTextAndOneShutdownAtTheEnd) {
    m_model.applyLinkDetail(LinkDetail::Live);
    m_model.applyLinkDetail(LinkDetail::AdapterWithoutVehicle);
    EXPECT_FALSE(m_model.countdownActive());
    const QSignalSpy spy(&m_model, &IgnitionOffShutdownModel::changed);
    m_clock.setMilliseconds(300000);
    m_model.tick();
    EXPECT_TRUE(m_model.countdownActive());
    EXPECT_EQ(m_model.secondsRemaining(), 60);
    EXPECT_EQ(m_model.countdownText(), QStringLiteral("Vehicle off: shutting down in 60 s"));
    EXPECT_EQ(spy.count(), 1);
    m_clock.setMilliseconds(359001);
    m_model.tick();
    EXPECT_EQ(m_model.secondsRemaining(), 1);
    EXPECT_EQ(m_executions, 0);
    m_clock.setMilliseconds(360000);
    m_model.tick();
    m_model.tick();
    EXPECT_EQ(m_executions, 1);
    EXPECT_EQ(m_lastCommand,
              (QStringList{QStringLiteral("systemctl"), QStringLiteral("poweroff")}));
    EXPECT_FALSE(m_model.countdownActive());
    EXPECT_EQ(m_shutdown.lastResultText(), QStringLiteral("Vehicle off: shutting down"));
}

TEST_F(IgnitionOffShutdownModelTest, CancelHidesTheBannerAndNothingRuns) {
    m_model.applyLinkDetail(LinkDetail::Live);
    m_model.applyLinkDetail(LinkDetail::LinkLostRetrying);
    m_clock.setMilliseconds(300000);
    m_model.tick();
    ASSERT_TRUE(m_model.countdownActive());
    m_model.cancel();
    EXPECT_FALSE(m_model.countdownActive());
    EXPECT_TRUE(m_model.countdownText().isEmpty());
    m_clock.setMilliseconds(10000000);
    m_model.tick();
    EXPECT_EQ(m_executions, 0);
}

TEST(NetworkAddressModelTest, OneSeveralOrNoAddresses) {
    QStringList addresses{QStringLiteral("172.20.10.2")};
    NetworkAddressModel model(QStringLiteral("lexus"), [&addresses]() {
        return addresses;
    });
    EXPECT_EQ(model.addressText(), QStringLiteral("SSH lexus@172.20.10.2"));
    const QSignalSpy spy(&model, &NetworkAddressModel::changed);
    addresses << QStringLiteral("10.0.0.94");
    model.refresh();
    EXPECT_EQ(model.addressText(), QStringLiteral("SSH lexus@172.20.10.2, lexus@10.0.0.94"));
    addresses.clear();
    model.refresh();
    model.refresh();
    EXPECT_EQ(model.addressText(), QStringLiteral("No network"));
    EXPECT_EQ(spy.count(), 2);
}

TEST(NetworkAddressModelTest, TheProductProviderNeverListsLoopback) {
    const QStringList addresses = NetworkAddressModel::interfaceAddresses()();
    EXPECT_FALSE(addresses.contains(QStringLiteral("127.0.0.1")));
}

} // namespace
// NOLINTEND(misc-include-cleaner)

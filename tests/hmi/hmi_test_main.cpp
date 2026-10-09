// Qt declares its macros in internal headers; the public ones are included, and QtQuickTest
// provides the test main macro.
// NOLINTBEGIN(misc-include-cleaner)
#include "fake_diagnostics_driver.h"
#include "fake_power_status_reader.h"
#include "fake_vehicle_data_view_model.h"

#include "lexus_head_unit/hmi/diagnostics_view_model.h"
#include "lexus_head_unit/hub/power_status_model.h"

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QtQuickTest>

// Registers the context properties the screens use before each QML test: the fake vehicle data
// view model, the real diagnostics view model with a driver that delivers reports, and the real
// power status model over a fake reader.
class HmiTestSetup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        m_power.startPolling(100);
        QQmlContext* context = engine->rootContext();
        context->setContextProperty(QStringLiteral("vehicleData"), &m_viewModel);
        context->setContextProperty(QStringLiteral("diagnosticsContext"), &m_diagnostics);
        context->setContextProperty(QStringLiteral("diagnosticsDriver"), &m_driver);
        context->setContextProperty(QStringLiteral("powerContext"), &m_power);
        context->setContextProperty(QStringLiteral("powerReaderContext"), &m_powerReader);
    }

private:
    lexus_head_unit::testing::FakeVehicleDataViewModel m_viewModel;
    lexus_head_unit::DiagnosticsViewModel m_diagnostics;
    lexus_head_unit::testing::FakeDiagnosticsDriver m_driver{m_diagnostics};
    lexus_head_unit::testing::FakePowerStatusReader m_powerReader;
    lexus_head_unit::PowerStatusModel m_power{m_powerReader};
};

QUICK_TEST_MAIN_WITH_SETUP(lexus_head_unit_hmi, HmiTestSetup)

// NOLINTEND(misc-include-cleaner)

#include "hmi_test_main.moc"

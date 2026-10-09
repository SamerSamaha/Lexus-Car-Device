// Qt declares its macros in internal headers; the public ones are included, and QtQuickTest
// provides the test main macro.
// NOLINTBEGIN(misc-include-cleaner)
#include "fake_vehicle_data_view_model.h"

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QtQuickTest>

// Registers the fake view model as the "vehicleData" context property before each QML test.
class HmiTestSetup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("vehicleData"), &m_viewModel);
    }

private:
    lexus_head_unit::testing::FakeVehicleDataViewModel m_viewModel;
};

QUICK_TEST_MAIN_WITH_SETUP(lexus_head_unit_hmi, HmiTestSetup)

// NOLINTEND(misc-include-cleaner)

#include "hmi_test_main.moc"

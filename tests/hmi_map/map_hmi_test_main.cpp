// Qt declares its macros in internal headers; the public ones are included, and QtQuickTest
// provides the test main macro.
// NOLINTBEGIN(misc-include-cleaner)
#include "map_test_driver.h"

#include "lexus_head_unit/hmi/map_view_model.h"

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QtQuickTest>

#include <memory>

// The real MapViewModel with an invented home area (in Lake Michigan, no real address), driven
// by the test driver: the context properties lexus-map sets, so Main.qml is tested as built.
class MapTestSetup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        m_model = std::make_unique<lexus_head_unit::MapViewModel>(
            lexus_head_unit::HomeArea{43.5, -87.0, 500.0}, false, [this]() {
                return m_driver.recording();
            });
        m_driver.setModel(*m_model);
        engine->rootContext()->setContextProperty(QStringLiteral("mapContext"), m_model.get());
        engine->rootContext()->setContextProperty(QStringLiteral("mapDriverContext"), &m_driver);
    }

private:
    lexus_head_unit::testing::MapTestDriver m_driver;
    std::unique_ptr<lexus_head_unit::MapViewModel> m_model;
};

QUICK_TEST_MAIN_WITH_SETUP(lexus_head_unit_map_hmi, MapTestSetup)

// NOLINTEND(misc-include-cleaner)

#include "map_hmi_test_main.moc"

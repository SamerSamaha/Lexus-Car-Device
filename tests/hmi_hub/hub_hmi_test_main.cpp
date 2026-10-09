// Qt declares its macros in internal headers; the public ones are included, and QtQuickTest
// provides the test main macro.
// NOLINTBEGIN(misc-include-cleaner)
#include "fake_process_launcher.h"

#include "lexus_head_unit/hub/app_process_manager.h"
#include "lexus_head_unit/hub/app_registry.h"
#include "lexus_head_unit/hub/hub_view_model.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/key_value_configuration.h"

#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QtQuickTest>

#include <memory>
#include <string>

namespace {

// Eight native apps named "App 1" to "App 8".
std::string eightAppRegistry() {
    std::string text = "[hub]\napps = a1, a2, a3, a4, a5, a6, a7, a8\n";
    for (int number = 1; number <= 8; ++number) {
        const std::string appId = "a" + std::to_string(number);
        text += "[app.";
        text += appId;
        text += "]\nname = App ";
        text += std::to_string(number);
        text += "\nkind = native\ncommand = /usr/bin/";
        text += appId;
        text += "\n";
    }
    return text;
}

} // namespace

// The real HubViewModel and AppProcessManager over a fake launcher: the QML talks to the same
// view model as in the product, and no process is started.
class HubTestSetup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        lexus_head_unit::KeyValueConfiguration configuration;
        configuration.loadFromText(eightAppRegistry());
        const lexus_head_unit::AppRegistry registry =
            lexus_head_unit::AppRegistry::fromConfiguration(configuration);
        m_manager = std::make_unique<lexus_head_unit::AppProcessManager>(m_launcher, m_clock);
        m_hub = std::make_unique<lexus_head_unit::HubViewModel>(
            *m_manager, registry.entries(), QStringList());
        m_hub->startPolling(20);
        engine->rootContext()->setContextProperty(QStringLiteral("hubContext"), m_hub.get());
    }

private:
    lexus_head_unit::SteadyClock m_clock;
    lexus_head_unit::testing::FakeProcessLauncher m_launcher;
    std::unique_ptr<lexus_head_unit::AppProcessManager> m_manager;
    std::unique_ptr<lexus_head_unit::HubViewModel> m_hub;
};

QUICK_TEST_MAIN_WITH_SETUP(lexus_head_unit_hub_hmi, HubTestSetup)

// NOLINTEND(misc-include-cleaner)

#include "hub_hmi_test_main.moc"

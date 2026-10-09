#include "lexus_head_unit/hub/power_status_model.h"

#include "lexus_head_unit/service/power_status.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Qt declares its macros, enumerations and QStringLiteral in internal headers; the public
// ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

ProcessPowerStatusReader::ProcessPowerStatusReader(QString program,
                                                   QStringList arguments,
                                                   QObject* parent)
    : PowerStatusReader(parent),
      m_program(std::move(program)),
      m_arguments(std::move(arguments)),
      m_process(this) {
    connect(&m_process,
            &QProcess::finished,
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                const QString output = QString::fromUtf8(m_process.readAllStandardOutput());
                emit readingReady(output, exitStatus == QProcess::NormalExit && exitCode == 0);
            });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit readingReady(QString(), false);
        }
    });
}

void ProcessPowerStatusReader::requestReading() {
    m_process.start(m_program, m_arguments);
}

bool ProcessPowerStatusReader::busy() const {
    return m_process.state() != QProcess::NotRunning;
}

PowerStatusModel::PowerStatusModel(PowerStatusReader& reader, QObject* parent)
    : QObject(parent), m_reader(&reader) {
    connect(m_reader, &PowerStatusReader::readingReady, this, &PowerStatusModel::onReading);
    connect(&m_timer, &QTimer::timeout, this, &PowerStatusModel::poll);
}

bool PowerStatusModel::available() const {
    return m_available;
}

bool PowerStatusModel::underVoltage() const {
    return m_flags.underVoltage;
}

bool PowerStatusModel::frequencyCapped() const {
    return m_flags.frequencyCapped;
}

bool PowerStatusModel::throttled() const {
    return m_flags.throttled;
}

bool PowerStatusModel::softTemperatureLimit() const {
    return m_flags.softTemperatureLimit;
}

bool PowerStatusModel::occurredSinceBoot() const {
    return m_flags.anyOccurred();
}

QString PowerStatusModel::flagsText() const {
    if (!m_available) {
        return QStringLiteral("Power status unavailable");
    }
    const std::vector<std::string> names = currentFlagNames(m_flags);
    if (names.empty()) {
        return QStringLiteral("Power OK");
    }
    QStringList texts;
    for (const std::string& name : names) {
        texts << QString::fromStdString(name);
    }
    return texts.join(QStringLiteral(", "));
}

std::uint64_t PowerStatusModel::skippedPolls() const {
    return m_skippedPolls;
}

void PowerStatusModel::startPolling(int intervalMilliseconds) {
    poll();
    m_timer.start(intervalMilliseconds);
}

void PowerStatusModel::poll() {
    if (m_reader->busy()) {
        ++m_skippedPolls;
        return;
    }
    m_reader->requestReading();
}

void PowerStatusModel::onReading(const QString& output, bool succeeded) {
    const std::optional<PowerFlags> flags =
        succeeded ? decodeGetThrottled(output.toStdString()) : std::nullopt;
    const bool available = flags.has_value();
    const PowerFlags next = flags.value_or(PowerFlags{});
    if (available == m_available && next.raw == m_flags.raw) {
        return;
    }
    m_available = available;
    m_flags = next;
    emit changed();
}

ShutdownController::ShutdownController(QStringList command,
                                       Executor executor,
                                       int armWindowMilliseconds,
                                       QObject* parent)
    : QObject(parent), m_command(std::move(command)), m_executor(std::move(executor)) {
    m_armTimer.setSingleShot(true);
    m_armTimer.setInterval(armWindowMilliseconds);
    connect(&m_armTimer, &QTimer::timeout, this, &ShutdownController::disarm);
}

bool ShutdownController::armed() const {
    return m_armed;
}

QString ShutdownController::lastResultText() const {
    return m_lastResultText;
}

int ShutdownController::executions() const {
    return m_executions;
}

void ShutdownController::press() {
    if (!m_armed) {
        m_armed = true;
        m_lastResultText = QStringLiteral("Tap again to shut down");
        m_armTimer.start();
        emit changed();
        return;
    }
    m_armTimer.stop();
    m_armed = false;
    if (m_command.isEmpty()) {
        m_lastResultText = QStringLiteral("No shutdown command configured");
    } else {
        ++m_executions;
        const bool started = m_executor(m_command.front(), m_command.mid(1));
        m_lastResultText = started ? QStringLiteral("Shutting down")
                                   : QStringLiteral("Shutdown command could not start");
    }
    emit changed();
}

void ShutdownController::disarm() {
    if (!m_armed) {
        return;
    }
    m_armed = false;
    m_lastResultText.clear();
    emit changed();
}

ShutdownController::Executor ShutdownController::detachedProcessExecutor() {
    return [](const QString& program, const QStringList& arguments) {
        return QProcess::startDetached(program, arguments);
    };
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)

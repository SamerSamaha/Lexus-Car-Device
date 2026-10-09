#pragma once

#include "lexus_head_unit/service/power_status.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <functional>

namespace lexus_head_unit {

// Produces one reading of the firmware flags at a time (DN-025). The real one runs a command
// asynchronously; tests use a fake.
class PowerStatusReader : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    virtual void requestReading() = 0;
    [[nodiscard]] virtual bool busy() const = 0;

signals:
    void readingReady(QString output, bool succeeded);
};

// Runs a command (default "vcgencmd get_throttled") with QProcess, without blocking.
class ProcessPowerStatusReader final : public PowerStatusReader {
    Q_OBJECT

public:
    ProcessPowerStatusReader(QString program, QStringList arguments, QObject* parent = nullptr);
    void requestReading() override;
    [[nodiscard]] bool busy() const override;

private:
    QString m_program;
    QStringList m_arguments;
    QProcess m_process;
};

// The flags for the hub's status strip (REQ-020): polls the reader, decodes the output.
class PowerStatusModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool underVoltage READ underVoltage NOTIFY changed)
    Q_PROPERTY(bool frequencyCapped READ frequencyCapped NOTIFY changed)
    Q_PROPERTY(bool throttled READ throttled NOTIFY changed)
    Q_PROPERTY(bool softTemperatureLimit READ softTemperatureLimit NOTIFY changed)
    Q_PROPERTY(bool occurredSinceBoot READ occurredSinceBoot NOTIFY changed)
    Q_PROPERTY(QString flagsText READ flagsText NOTIFY changed)

public:
    explicit PowerStatusModel(PowerStatusReader& reader, QObject* parent = nullptr);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool underVoltage() const;
    [[nodiscard]] bool frequencyCapped() const;
    [[nodiscard]] bool throttled() const;
    [[nodiscard]] bool softTemperatureLimit() const;
    [[nodiscard]] bool occurredSinceBoot() const;
    // The current flags joined ("Under-voltage, Throttled"), "Power OK", or
    // "Power status unavailable".
    [[nodiscard]] QString flagsText() const;
    [[nodiscard]] std::uint64_t skippedPolls() const;

    void startPolling(int intervalMilliseconds);
    Q_INVOKABLE void poll();

signals:
    void changed();

private:
    void onReading(const QString& output, bool succeeded);

    PowerStatusReader* m_reader;
    QTimer m_timer;
    bool m_available = false;
    PowerFlags m_flags;
    std::uint64_t m_skippedPolls = 0;
};

// The two-tap shutdown control (DN-025): the first press arms it for a window, the second
// press within the window runs the shutdown command through the executor.
class ShutdownController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool armed READ armed NOTIFY changed)
    Q_PROPERTY(QString lastResultText READ lastResultText NOTIFY changed)
    Q_PROPERTY(int executions READ executions NOTIFY changed)

public:
    // Starts program with arguments, detached; true if it started.
    using Executor = std::function<bool(const QString& program, const QStringList& arguments)>;

    ShutdownController(QStringList command,
                       Executor executor,
                       int armWindowMilliseconds = 5000,
                       QObject* parent = nullptr);

    [[nodiscard]] bool armed() const;
    [[nodiscard]] QString lastResultText() const;
    [[nodiscard]] int executions() const;
    Q_INVOKABLE void press();

    // The executor used in the product: QProcess::startDetached.
    static Executor detachedProcessExecutor();

signals:
    void changed();

private:
    void disarm();

    QStringList m_command;
    Executor m_executor;
    QTimer m_armTimer;
    bool m_armed = false;
    QString m_lastResultText;
    int m_executions = 0;
};

} // namespace lexus_head_unit

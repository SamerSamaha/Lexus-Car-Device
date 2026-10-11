#pragma once

#include "lexus_head_unit/hub/car_status_models.h"

#include <QFile>
#include <QObject>
#include <QString>
#include <QTemporaryDir>

namespace lexus_head_unit::testing {

// Plays lexus-screen-record for the QML tests of REQ-026: writes and removes the status file
// that the hub's RecordingStatusModel reads, then asks the model to read it at once.
class RecordingTestDriver : public QObject {
    Q_OBJECT

public:
    explicit RecordingTestDriver(QObject* parent = nullptr) : QObject(parent) {}

    [[nodiscard]] QString statusPath() const {
        return m_folder.filePath(QStringLiteral("lexus-screen-recording"));
    }

    void setModel(RecordingStatusModel& model) {
        m_model = &model;
    }

    Q_INVOKABLE void start(const QString& fileName) {
        QFile status(statusPath());
        if (status.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            status.write(m_folder.filePath(fileName).toUtf8() + "\n");
        }
        status.close();
        refresh();
    }

    Q_INVOKABLE void stop() {
        QFile::remove(statusPath());
        refresh();
    }

private:
    void refresh() {
        if (m_model != nullptr) {
            m_model->refresh();
        }
    }

    QTemporaryDir m_folder;
    RecordingStatusModel* m_model = nullptr;
};

} // namespace lexus_head_unit::testing

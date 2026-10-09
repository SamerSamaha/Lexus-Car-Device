#pragma once

#include "lexus_head_unit/hub/power_status_model.h"

#include <QObject>
#include <QString>

namespace lexus_head_unit::testing {

// A PowerStatusReader for tests. By default each request is answered at once with the
// current output; with holdReplies(true) a request stays open until answer() is called.
class FakePowerStatusReader final : public PowerStatusReader {
    Q_OBJECT

public:
    using PowerStatusReader::PowerStatusReader;

    void requestReading() override {
        ++m_requests;
        if (m_holdReplies) {
            m_busy = true;
            return;
        }
        emit readingReady(m_output, m_succeeds);
    }

    [[nodiscard]] bool busy() const override {
        return m_busy;
    }

    Q_INVOKABLE void setOutput(const QString& output, bool succeeds = true) {
        m_output = output;
        m_succeeds = succeeds;
    }

    void holdReplies(bool hold) {
        m_holdReplies = hold;
    }

    void answer() {
        m_busy = false;
        emit readingReady(m_output, m_succeeds);
    }

    [[nodiscard]] int requests() const {
        return m_requests;
    }

private:
    QString m_output = QStringLiteral("throttled=0x0\n");
    bool m_succeeds = true;
    bool m_holdReplies = false;
    bool m_busy = false;
    int m_requests = 0;
};

} // namespace lexus_head_unit::testing

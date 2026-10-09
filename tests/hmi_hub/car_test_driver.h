#pragma once

#include "lexus_head_unit/hub/car_status_models.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <QObject>

#include <cstdint>

namespace lexus_head_unit::testing {

// Lets the QML tests of car mode (DN-043) move the hub's countdown model through its phases on
// a manual clock: live, then quiet past the quiet time.
class CarTestDriver : public QObject {
    Q_OBJECT

public:
    CarTestDriver(IgnitionOffShutdownModel& model,
                  ManualClock& clock,
                  const int& executions,
                  std::int64_t quietMilliseconds,
                  QObject* parent = nullptr)
        : QObject(parent),
          m_model(&model),
          m_clock(&clock),
          m_executions(&executions),
          m_quietMilliseconds(quietMilliseconds) {}

    [[nodiscard]] Q_INVOKABLE int shutdownExecutions() const {
        return *m_executions;
    }

    // Live again: any countdown or cancellation ends.
    Q_INVOKABLE void reset() {
        m_model->applyLinkDetail(LinkDetail::Live);
    }

    Q_INVOKABLE void startCountdown() {
        m_model->applyLinkDetail(LinkDetail::Live);
        m_model->applyLinkDetail(LinkDetail::AdapterWithoutVehicle);
        m_clock->advanceMilliseconds(m_quietMilliseconds);
        m_model->tick();
    }

private:
    IgnitionOffShutdownModel* m_model;
    ManualClock* m_clock;
    const int* m_executions;
    std::int64_t m_quietMilliseconds;
};

} // namespace lexus_head_unit::testing

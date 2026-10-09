#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QMetaType>

// Declares the service-layer value types to Qt's meta-type system once, so they can travel
// through queued signal connections. Included before any Q_OBJECT class that uses them.
Q_DECLARE_METATYPE(lexus_head_unit::SignalSample)
Q_DECLARE_METATYPE(lexus_head_unit::ConnectionTransition)

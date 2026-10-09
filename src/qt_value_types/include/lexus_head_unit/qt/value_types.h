#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QMetaType>

// Declares the service-layer value types to Qt's meta-type system once, so they can travel
// through queued signal connections and Qt signals of the D-Bus client. Included before any
// Q_OBJECT class that uses them. Shared by the view models and the D-Bus client (DN-022), so
// that neither has to link the other.
Q_DECLARE_METATYPE(lexus_head_unit::SignalSample)
Q_DECLARE_METATYPE(lexus_head_unit::ConnectionTransition)

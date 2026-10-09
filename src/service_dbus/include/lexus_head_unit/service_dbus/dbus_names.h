#pragma once

#include <QString>

namespace lexus_head_unit::dbus_names {

// DN-022. The trailing 1 is the interface version; an incompatible change adds VehicleData2.
inline QString serviceName() {
    return QStringLiteral("io.github.samersamaha.LexusHeadUnit");
}

inline QString objectPath() {
    return QStringLiteral("/io/github/samersamaha/LexusHeadUnit/VehicleData");
}

inline QString interfaceName() {
    return QStringLiteral("io.github.samersamaha.LexusHeadUnit.VehicleData1");
}

constexpr unsigned int interfaceVersion = 1;

} // namespace lexus_head_unit::dbus_names

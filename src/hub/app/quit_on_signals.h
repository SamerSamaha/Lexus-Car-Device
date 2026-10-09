#pragma once

namespace lexus_head_unit::hub_app {

// Makes SIGTERM and SIGINT quit the Qt event loop instead of ending the process at once, so
// that main() can stop the foreground app before the hub exits (DN-021, failure cases). The
// signal handler only writes one byte to a socket pair; a QSocketNotifier on the other end
// calls QCoreApplication::quit() on the UI thread. Returns false if the socket pair or the
// handlers could not be set up. Call once, after the application object exists.
bool installQuitOnSignals();

} // namespace lexus_head_unit::hub_app

#pragma once

namespace lexus_head_unit {

// Makes SIGTERM and SIGINT quit the Qt event loop instead of ending the process at once, so
// that main() can clean up first: the hub stops its foreground app (DN-021), the service stops
// its worker loop and its source (DN-022). The
// signal handler only writes one byte to a socket pair; a QSocketNotifier on the other end
// calls QCoreApplication::quit() on the UI thread. Returns false if the socket pair or the
// handlers could not be set up. Call once, after the application object exists.
bool installQuitOnSignals();

} // namespace lexus_head_unit

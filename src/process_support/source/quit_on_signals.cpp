#include "lexus_head_unit/process_support/quit_on_signals.h"

#include <QCoreApplication>
#include <QSocketNotifier>

#include <array>
#include <csignal>

#include <sys/socket.h>
#include <unistd.h>

// glibc and Qt declare these symbols in internal headers; the public headers above are the
// right ones to include.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

// Written only by the signal handler and read only at setup, as the self-pipe pattern needs.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<int, 2> signalSocketPair = {-1, -1};

void onSignal(int /*signalNumber*/) {
    const char wakeByte = 1;
    // write() is async-signal-safe; the result is ignored because nothing else can be done here.
    static_cast<void>(::write(signalSocketPair.at(0), &wakeByte, 1));
}

} // namespace

bool installQuitOnSignals() {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalSocketPair.data()) != 0) {
        return false;
    }
    // The application object is the parent and deletes the notifier.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto* notifier = new QSocketNotifier(
        signalSocketPair.at(1), QSocketNotifier::Read, QCoreApplication::instance());
    QObject::connect(notifier, &QSocketNotifier::activated, notifier, [notifier]() {
        notifier->setEnabled(false);
        char discarded = 0;
        static_cast<void>(::read(signalSocketPair.at(1), &discarded, 1));
        QCoreApplication::quit();
    });
    struct sigaction action{};
    action.sa_handler = onSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    return ::sigaction(SIGTERM, &action, nullptr) == 0 &&
           ::sigaction(SIGINT, &action, nullptr) == 0;
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)

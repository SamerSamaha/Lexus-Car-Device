// A stand-in app for the hub tests (DN-021, test plan rows 4 and 5). Options:
//   --run-ms <n>          exit after n milliseconds (default 50)
//   --exit-code <n>       exit status (default 0)
//   --forever             run until a signal ends it
//   --ignore-term         ignore SIGTERM, so only SIGKILL ends it
//   --child-pid-file <p>  start a child in the same process group that sleeps until killed,
//                         and write its process ID to p
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#include <unistd.h>

// NOLINTBEGIN(misc-include-cleaner)
namespace {

constexpr int defaultRunMilliseconds = 50;
constexpr int sleepSliceMilliseconds = 10;

struct Options {
    int runMilliseconds = defaultRunMilliseconds;
    int exitCode = 0;
    bool forever = false;
    bool ignoreTerm = false;
    std::string childProcessIdFile;
};

int toInteger(const char* text) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    return end != text && *end == 0 ? static_cast<int>(value) : 0;
}

Options parse(int argumentCount, char** argumentValues) {
    Options options;
    for (int index = 1; index < argumentCount; ++index) {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const std::string argument = argumentValues[index];
        const bool hasValue = index + 1 < argumentCount;
        if (argument == "--run-ms" && hasValue) {
            options.runMilliseconds = toInteger(argumentValues[++index]);
        } else if (argument == "--exit-code" && hasValue) {
            options.exitCode = toInteger(argumentValues[++index]);
        } else if (argument == "--child-pid-file" && hasValue) {
            options.childProcessIdFile = argumentValues[++index];
        } else if (argument == "--forever") {
            options.forever = true;
        } else if (argument == "--ignore-term") {
            options.ignoreTerm = true;
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return options;
}

void sleepForever() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepSliceMilliseconds));
    }
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const Options options = parse(argumentCount, argumentValues);
    if (options.ignoreTerm) {
        // NOLINTNEXTLINE(cert-err33-c)
        std::signal(SIGTERM, SIG_IGN);
    }
    if (!options.childProcessIdFile.empty()) {
        const pid_t child = ::fork();
        if (child == 0) {
            sleepForever();
        }
        std::ofstream file(options.childProcessIdFile);
        file << child << "\n";
    }
    if (options.forever || options.ignoreTerm) {
        sleepForever();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(options.runMilliseconds));
    return options.exitCode;
}
// NOLINTEND(misc-include-cleaner)

#pragma once

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/vehicle_data_source.h"
#include "source_harness.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace lexus_head_unit::testing {

// The real source over the emulator. The emulator's values are its own, so "produce a sample"
// means "poll until that signal has been answered", and the link is broken by killing the
// emulator and restored by restarting it at the same link path.
class Elm327SourceHarness final : public SourceHarness {
public:
    [[nodiscard]] std::string name() const override {
        return "elm327";
    }

    std::unique_ptr<VehicleDataSource> makeSource(const Clock& clock) override {
        if (!m_emulator.start()) {
            ADD_FAILURE() << "emulator did not start: " << m_emulator.lastError();
        }
        m_transport = std::make_unique<FileDescriptorByteTransport>(m_emulator.linkPath());
        Elm327SourceConfiguration configuration;
        configuration.devicePath = m_emulator.linkPath();
        configuration.replyTimeoutMilliseconds = 2000;
        auto source = std::make_unique<Elm327ObdSource>(*m_transport, clock, configuration);
        m_source = source.get();
        return source;
    }

    void produceSample(SignalId signalId, double /*value*/) override {
        m_pendingSignals.push_back(signalId);
    }

    void breakLink() override {
        m_emulator.kill();
        m_linkBroken = true;
    }

    void restoreLink() override {
        if (!m_emulator.start()) {
            ADD_FAILURE() << "emulator did not restart: " << m_emulator.lastError();
        }
        m_linkBroken = false;
    }

    int drain(VehicleDataSource& source) override {
        int calls = 0;
        if (m_linkBroken) {
            // One poll is enough: the dead device reads end-of-file.
            source.runOnce();
            return 1;
        }
        // Reconnect if the source is waiting for its backoff; the clock is manual in the suite,
        // so the test advances it until the attempt is due.
        for (int attempt = 0;
             attempt < 50 && source.connectionState() != ConnectionState::Connected;
             ++attempt) {
            source.runOnce();
            ++calls;
        }
        // Poll a full cycle so every pending signal has been requested at least once.
        const int cycle = static_cast<int>(m_source->pollList().size());
        for (int poll = 0; poll < cycle * 2 && !m_pendingSignals.empty(); ++poll) {
            source.runOnce();
            ++calls;
        }
        m_pendingSignals.clear();
        return calls;
    }

    EmulatorProcess& emulator() {
        return m_emulator;
    }

private:
    EmulatorProcess m_emulator;
    std::unique_ptr<FileDescriptorByteTransport> m_transport;
    Elm327ObdSource* m_source = nullptr;
    std::vector<SignalId> m_pendingSignals;
    bool m_linkBroken = false;
};

} // namespace lexus_head_unit::testing

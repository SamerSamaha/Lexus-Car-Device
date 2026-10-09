#pragma once

#include "lexus_head_unit/service/key_value_configuration.h"

#include <cstdint>
#include <string>
#include <vector>

namespace lexus_head_unit {

struct Elm327SourceConfiguration {
    std::string devicePath = "/dev/rfcomm0";
    std::int64_t replyTimeoutMilliseconds = 1000;
    std::int64_t linkLossTimeoutMilliseconds = 2000;
    std::int64_t pollIntervalMilliseconds = 0;
    std::vector<std::int64_t> backoffScheduleMilliseconds = {1000, 2000, 4000, 8000};
    std::int64_t backoffCapMilliseconds = 10000;

    // Keys: elm327.device, elm327.reply_timeout_ms, elm327.link_loss_timeout_ms,
    // elm327.poll_interval_ms, elm327.backoff_cap_ms. Missing or invalid keys keep the default.
    static Elm327SourceConfiguration fromConfiguration(const KeyValueConfiguration& configuration);
};

} // namespace lexus_head_unit

#include "lexus_head_unit/hardware/elm327_source_configuration.h"

#include "lexus_head_unit/service/key_value_configuration.h"

#include <cstdint>

namespace lexus_head_unit {

namespace {

std::int64_t positiveOrDefault(std::int64_t value, std::int64_t defaultValue) {
    return value > 0 ? value : defaultValue;
}

} // namespace

Elm327SourceConfiguration
Elm327SourceConfiguration::fromConfiguration(const KeyValueConfiguration& configuration) {
    Elm327SourceConfiguration result;
    result.devicePath = configuration.stringValue("elm327.device", result.devicePath);
    result.replyTimeoutMilliseconds = positiveOrDefault(
        configuration.integerValue("elm327.reply_timeout_ms", result.replyTimeoutMilliseconds),
        result.replyTimeoutMilliseconds);
    result.linkLossTimeoutMilliseconds =
        positiveOrDefault(configuration.integerValue("elm327.link_loss_timeout_ms",
                                                     result.linkLossTimeoutMilliseconds),
                          result.linkLossTimeoutMilliseconds);
    const std::int64_t pollInterval =
        configuration.integerValue("elm327.poll_interval_ms", result.pollIntervalMilliseconds);
    result.pollIntervalMilliseconds = pollInterval >= 0 ? pollInterval : 0;
    result.backoffCapMilliseconds = positiveOrDefault(
        configuration.integerValue("elm327.backoff_cap_ms", result.backoffCapMilliseconds),
        result.backoffCapMilliseconds);
    return result;
}

} // namespace lexus_head_unit

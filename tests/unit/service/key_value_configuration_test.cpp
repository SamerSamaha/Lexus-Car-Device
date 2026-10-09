// Verifies: REQ-002

#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/service/key_value_configuration.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::KeyValueConfiguration;

constexpr const char* sampleText = R"(# Head unit configuration
[source]
kind = elm327

[elm327]
device = /tmp/obd
reply_timeout_ms = 1500
link_loss_timeout_ms=3000
poll_interval_ms = 20

[staleness]
default_ms = 1000
vehicle_speed_ms = 250
bad = twelve
)";

TEST(KeyValueConfigurationTest, ParsesSectionsKeysCommentsAndWhitespace) {
    KeyValueConfiguration configuration;
    configuration.loadFromText(sampleText);
    EXPECT_EQ(configuration.stringValue("source.kind", "fake"), "elm327");
    EXPECT_EQ(configuration.stringValue("elm327.device", ""), "/tmp/obd");
    EXPECT_EQ(configuration.integerValue("elm327.reply_timeout_ms", 0), 1500);
    EXPECT_EQ(configuration.integerValue("elm327.link_loss_timeout_ms", 0), 3000);
    EXPECT_EQ(configuration.integerValue("staleness.vehicle_speed_ms", 0), 250);
    EXPECT_TRUE(configuration.contains("staleness.default_ms"));
    EXPECT_FALSE(configuration.contains("staleness.engine_rpm_ms"));
    EXPECT_EQ(configuration.entries().size(), 8U);
}

TEST(KeyValueConfigurationTest, MissingOrInvalidValuesGiveTheDefault) {
    KeyValueConfiguration configuration;
    configuration.loadFromText(sampleText);
    EXPECT_EQ(configuration.stringValue("missing.key", "default"), "default");
    EXPECT_EQ(configuration.integerValue("staleness.bad", 7), 7);
    EXPECT_EQ(configuration.integerValue("missing.key", 9), 9);
}

TEST(KeyValueConfigurationTest, KeysWithoutASectionAndLaterKeysOverride) {
    KeyValueConfiguration configuration;
    configuration.loadFromText("name = first\nname = second\n[a]\nx=1\n[b]\nx = 2\n; comment\n");
    EXPECT_EQ(configuration.stringValue("name", ""), "second");
    EXPECT_EQ(configuration.integerValue("a.x", 0), 1);
    EXPECT_EQ(configuration.integerValue("b.x", 0), 2);
}

TEST(KeyValueConfigurationTest, LoadsFromAFileAndReportsAMissingFile) {
    const std::string path = std::string(::testing::TempDir()) + "lexus_head_unit_config_test.conf";
    {
        std::ofstream file(path);
        file << "[source]\nkind = fake\n";
    }
    KeyValueConfiguration configuration;
    EXPECT_TRUE(configuration.loadFromFile(path));
    EXPECT_EQ(configuration.stringValue("source.kind", ""), "fake");
    EXPECT_EQ(std::remove(path.c_str()), 0);
    KeyValueConfiguration missing;
    EXPECT_FALSE(missing.loadFromFile("/nonexistent/lexus-head-unit.conf"));
}

TEST(Elm327SourceConfigurationTest, TypedValuesComeFromTheFileWithDefaultsForTheRest) {
    KeyValueConfiguration configuration;
    configuration.loadFromText(sampleText);
    const Elm327SourceConfiguration typed =
        Elm327SourceConfiguration::fromConfiguration(configuration);
    EXPECT_EQ(typed.devicePath, "/tmp/obd");
    EXPECT_EQ(typed.replyTimeoutMilliseconds, 1500);
    EXPECT_EQ(typed.linkLossTimeoutMilliseconds, 3000);
    EXPECT_EQ(typed.pollIntervalMilliseconds, 20);
    EXPECT_EQ(typed.backoffCapMilliseconds, 10000);
    EXPECT_EQ(typed.backoffScheduleMilliseconds,
              (std::vector<std::int64_t>{1000, 2000, 4000, 8000}));
}

TEST(Elm327SourceConfigurationTest, NonPositiveTimeoutsKeepTheDefaults) {
    KeyValueConfiguration configuration;
    configuration.loadFromText("[elm327]\nreply_timeout_ms = 0\nlink_loss_timeout_ms = -5\n"
                               "poll_interval_ms = -1\nbackoff_cap_ms = 0\n");
    const Elm327SourceConfiguration typed =
        Elm327SourceConfiguration::fromConfiguration(configuration);
    EXPECT_EQ(typed.replyTimeoutMilliseconds, 1000);
    EXPECT_EQ(typed.linkLossTimeoutMilliseconds, 2000);
    EXPECT_EQ(typed.pollIntervalMilliseconds, 0);
    EXPECT_EQ(typed.backoffCapMilliseconds, 10000);
}

TEST(Elm327SourceConfigurationTest, DiscoveryAndVehicleSilenceTimeoutsAreReadOrDefaulted) {
    KeyValueConfiguration configuration;
    configuration.loadFromText(
        "[elm327]\ndiscovery_timeout_ms = 12000\nvehicle_silence_timeout_ms = 8000\n");
    const Elm327SourceConfiguration typed =
        Elm327SourceConfiguration::fromConfiguration(configuration);
    EXPECT_EQ(typed.discoveryTimeoutMilliseconds, 12000);
    EXPECT_EQ(typed.vehicleSilenceTimeoutMilliseconds, 8000);
    KeyValueConfiguration invalid;
    invalid.loadFromText("[elm327]\ndiscovery_timeout_ms = 0\nvehicle_silence_timeout_ms = -1\n");
    const Elm327SourceConfiguration defaulted =
        Elm327SourceConfiguration::fromConfiguration(invalid);
    EXPECT_EQ(defaulted.discoveryTimeoutMilliseconds, 10000);
    EXPECT_EQ(defaulted.vehicleSilenceTimeoutMilliseconds, 5000);
}

TEST(KeyValueConfigurationSetValueTest, SetValueAddsAndOverrides) {
    lexus_head_unit::KeyValueConfiguration configuration;
    configuration.loadFromText("[record]\nfile = a.rec\n");
    configuration.setValue("record.file", "b.rec");
    configuration.setValue("replay.timing", "fast");
    EXPECT_EQ(configuration.stringValue("record.file", ""), "b.rec");
    EXPECT_EQ(configuration.stringValue("replay.timing", ""), "fast");
}

} // namespace

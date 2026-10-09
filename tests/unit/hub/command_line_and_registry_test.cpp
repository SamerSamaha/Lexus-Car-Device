// Verifies: REQ-016

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/app_registry.h"
#include "lexus_head_unit/hub/command_line.h"
#include "lexus_head_unit/service/key_value_configuration.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using lexus_head_unit::AppEntry;
using lexus_head_unit::AppKind;
using lexus_head_unit::AppRegistry;
using lexus_head_unit::KeyValueConfiguration;
using lexus_head_unit::RestartPolicy;
using lexus_head_unit::splitCommandLine;
using lexus_head_unit::toString;
using Words = std::vector<std::string>;

AppRegistry registryFrom(std::string_view text) {
    KeyValueConfiguration configuration;
    configuration.loadFromText(text);
    return AppRegistry::fromConfiguration(configuration);
}

TEST(CommandLineTest, SplitsOnWhitespaceAndKeepsQuotedWordsTogether) {
    EXPECT_EQ(splitCommandLine("lexus-head-unit --config /etc/a.conf"),
              (Words{"lexus-head-unit", "--config", "/etc/a.conf"}));
    EXPECT_EQ(splitCommandLine("  app\t\"two words\"  x\"y z\" "),
              (Words{"app", "two words", "xy z"}));
    EXPECT_EQ(splitCommandLine(R"(echo "a \"quoted\" \\ word")"),
              (Words{"echo", R"(a "quoted" \ word)"}));
    EXPECT_EQ(splitCommandLine(R"(app "" end)"), (Words{"app", "", "end"}));
}

TEST(CommandLineTest, EmptyTextGivesNoWordsAndAnUnclosedQuoteGivesNothing) {
    EXPECT_EQ(splitCommandLine(""), Words{});
    EXPECT_EQ(splitCommandLine("   "), Words{});
    EXPECT_EQ(splitCommandLine("app \"open"), std::nullopt);
    EXPECT_EQ(splitCommandLine("app \"ends in \\"), std::nullopt);
}

TEST(CommandLineTest, ShellCharactersAreOrdinaryCharacters) {
    EXPECT_EQ(splitCommandLine("app ; rm -rf $HOME | cat > x"),
              (Words{"app", ";", "rm", "-rf", "$HOME", "|", "cat", ">", "x"}));
}

TEST(AppRegistryTest, ReadsEntriesInTheOrderOfHubApps) {
    const AppRegistry registry = registryFrom(R"(
[hub]
apps = vehicle_data, music
browser_command = chromium --user-data-dir=/tmp/hub-browser --app={url}

[app.music]
name = Music
kind = url
url = https://music.example/play?a=1 b

[app.vehicle_data]
name = Vehicle data
kind = native
command = lexus-head-unit --config "/etc/lexus head unit.conf"
icon = /usr/share/icons/car.png
restart = on-failure
)");
    EXPECT_TRUE(registry.errors().empty());
    ASSERT_EQ(registry.entries().size(), 2U);
    const AppEntry& vehicleData = registry.entries().at(0);
    EXPECT_EQ(vehicleData.id, "vehicle_data");
    EXPECT_EQ(vehicleData.name, "Vehicle data");
    EXPECT_EQ(vehicleData.kind, AppKind::Native);
    EXPECT_EQ(vehicleData.arguments,
              (Words{"lexus-head-unit", "--config", "/etc/lexus head unit.conf"}));
    EXPECT_EQ(vehicleData.icon, "/usr/share/icons/car.png");
    EXPECT_EQ(vehicleData.restart, RestartPolicy::OnFailure);
    const AppEntry& music = registry.entries().at(1);
    EXPECT_EQ(music.kind, AppKind::Url);
    EXPECT_EQ(music.restart, RestartPolicy::Never);
    EXPECT_EQ(music.arguments,
              (Words{"chromium",
                     "--user-data-dir=/tmp/hub-browser",
                     "--app=https://music.example/play?a=1 b"}));
    EXPECT_EQ(registry.find("music"), &registry.entries().at(1));
    EXPECT_EQ(registry.find("absent"), nullptr);
}

TEST(AppRegistryTest, MissingAppListIsOneError) {
    const AppRegistry registry = registryFrom("[app.a]\nname = A\nkind = native\ncommand = a\n");
    EXPECT_TRUE(registry.entries().empty());
    EXPECT_EQ(registry.errors(), Words{"hub.apps is missing or empty"});
}

TEST(AppRegistryTest, EachBadEntryIsSkippedWithItsErrorAndTheRestLoad) {
    const AppRegistry registry = registryFrom(R"(
[hub]
apps = good, Bad!, good, nameless, oddkind, nocommand, quote, nourl, oddrestart, web
[app.good]
name = Good
kind = native
command = good-app
[app.nameless]
kind = native
command = x
[app.oddkind]
name = Odd
kind = flatpak
[app.nocommand]
name = No command
kind = native
[app.quote]
name = Quote
kind = native
command = app "open
[app.nourl]
name = No url
kind = url
[app.oddrestart]
name = Odd restart
kind = native
command = x
restart = always
[app.web]
name = Web
kind = url
url = https://example.org
)");
    ASSERT_EQ(registry.entries().size(), 1U);
    EXPECT_EQ(registry.entries().front().id, "good");
    const Words expectedErrors = {
        "app id 'Bad!' may use only a to z, 0 to 9, _ and -",
        "app 'good' is listed twice in hub.apps",
        "app 'nameless' has no name",
        "app 'oddkind' has an unknown kind 'flatpak' (native or url)",
        "app 'nocommand' has an empty command",
        "app 'quote' has an unclosed quote in its command",
        "app 'nourl' is a url app with no url",
        "app 'oddrestart' has an unknown restart policy 'always'",
        "app 'web' needs hub.browser_command containing {url}",
    };
    EXPECT_EQ(registry.errors(), expectedErrors);
}

TEST(AppRegistryTest, KindAndPolicyNames) {
    EXPECT_EQ(toString(AppKind::Native), "native");
    EXPECT_EQ(toString(AppKind::Url), "url");
    EXPECT_EQ(toString(RestartPolicy::Never), "never");
    EXPECT_EQ(toString(RestartPolicy::OnFailure), "on-failure");
}

} // namespace

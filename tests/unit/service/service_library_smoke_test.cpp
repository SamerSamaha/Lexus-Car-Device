#include "lexus_head_unit/service/library_version.h"

#include <gtest/gtest.h>

namespace {

TEST(ServiceLibrarySmokeTest, ReportsTheProjectVersion) {
    EXPECT_EQ(lexus_head_unit::serviceVersion(), "0.1.0");
}
} // namespace

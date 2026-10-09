// Verifies: REQ-021

#include "lexus_head_unit/hmi/diagnostics_view_model.h"
#include "lexus_head_unit/service/diagnostics_report.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::DiagnosticsReport;
using lexus_head_unit::DiagnosticsViewModel;
using lexus_head_unit::TroubleCode;

DiagnosticsReport reportWith(int codeCount) {
    DiagnosticsReport report;
    report.codesRead = true;
    for (int index = 0; index < codeCount; ++index) {
        report.troubleCodes.push_back(
            TroubleCode{"P013" + std::to_string(index), "Text " + std::to_string(index)});
    }
    report.identificationRead = true;
    report.vehicleIdentification = "DEMO-NOT-A-VIN";
    return report;
}

TEST(DiagnosticsViewModelTest, BeforeAnyReadEverythingSaysSo) {
    const DiagnosticsViewModel model;
    EXPECT_EQ(model.summaryText(), QStringLiteral("Not read yet"));
    EXPECT_EQ(model.identificationText(), QStringLiteral("Not read"));
    EXPECT_EQ(model.codeCount(), 0);
    EXPECT_TRUE(model.codes().isEmpty());
    EXPECT_FALSE(model.reading());
}

TEST(DiagnosticsViewModelTest, RefreshCallsTheRequesterAndShowsReadingUntilTheReport) {
    DiagnosticsViewModel model;
    int requests = 0;
    model.setRequester([&requests]() {
        ++requests;
    });
    const QSignalSpy changes(&model, &DiagnosticsViewModel::changed);
    model.refresh();
    EXPECT_EQ(requests, 1);
    EXPECT_TRUE(model.reading());
    EXPECT_EQ(model.summaryText(), QStringLiteral("Reading..."));
    model.onDiagnostics(reportWith(2));
    EXPECT_FALSE(model.reading());
    EXPECT_EQ(changes.count(), 2);
    EXPECT_TRUE(model.readTimeText().startsWith(QStringLiteral("Read at ")));
}

TEST(DiagnosticsViewModelTest, CountsCodesTextsAndIdentification) {
    DiagnosticsViewModel model;
    model.onDiagnostics(reportWith(2));
    EXPECT_EQ(model.summaryText(), QStringLiteral("2 stored trouble codes"));
    EXPECT_EQ(model.codes(), (QStringList{QStringLiteral("P0130"), QStringLiteral("P0131")}));
    EXPECT_EQ(model.descriptions(),
              (QStringList{QStringLiteral("Text 0"), QStringLiteral("Text 1")}));
    EXPECT_EQ(model.identificationText(), QStringLiteral("DEMO-NOT-A-VIN"));
    model.onDiagnostics(reportWith(1));
    EXPECT_EQ(model.summaryText(), QStringLiteral("1 stored trouble code"));
    model.onDiagnostics(reportWith(0));
    EXPECT_EQ(model.summaryText(), QStringLiteral("No stored trouble codes"));
    const DiagnosticsReport unread;
    model.onDiagnostics(unread);
    EXPECT_EQ(model.summaryText(), QStringLiteral("Trouble codes could not be read"));
    EXPECT_EQ(model.identificationText(), QStringLiteral("Not read"));
    EXPECT_EQ(model.codeCount(), 0);
}

TEST(DiagnosticsViewModelTest, RefreshWithoutARequesterDoesNothing) {
    DiagnosticsViewModel model;
    model.refresh();
    EXPECT_FALSE(model.reading());
}

} // namespace
// NOLINTEND(misc-include-cleaner)

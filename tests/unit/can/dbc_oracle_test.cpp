// Verifies: REQ-005

// Compares DbcDecoder with the cantools oracle (DN-027). tools/dbc_oracle.py, run by the
// DbcOracleGenerate test from the test-only virtual environment, writes the frames and the
// values cantools decoded; this test decodes the same frames and lists every mismatch in full.

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/dbc_database.h"
#include "lexus_head_unit/hardware/dbc_decoder.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::CanFrame;
using lexus_head_unit::DbcDatabase;
using lexus_head_unit::DbcDecoder;
using lexus_head_unit::DecodeKind;
using lexus_head_unit::DecodeResult;

constexpr double tolerance = 1e-6;
constexpr std::size_t minimumFrameCount = 10000;
constexpr int hexBase = 16;

struct OracleLine {
    std::size_t frameNumber = 0;
    std::uint32_t identifier = 0;
    bool extended = false;
    std::string dataHex;
    std::string signalName;
    double value = 0.0;
};

std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

std::vector<OracleLine> readOracle(const std::string& path) {
    std::vector<OracleLine> lines;
    std::ifstream file(path);
    std::string text;
    std::getline(file, text);
    while (std::getline(file, text)) {
        const std::vector<std::string> fields = splitCsv(text);
        if (fields.size() != 6) {
            continue;
        }
        OracleLine line;
        line.frameNumber = std::stoul(fields.at(0));
        line.identifier = static_cast<std::uint32_t>(std::stoul(fields.at(1)));
        line.extended = fields.at(2) == "1";
        line.dataHex = fields.at(3);
        line.signalName = fields.at(4);
        line.value = std::stod(fields.at(5));
        lines.push_back(line);
    }
    return lines;
}

CanFrame frameOf(const OracleLine& line) {
    CanFrame frame;
    frame.identifier = line.identifier;
    frame.extended = line.extended;
    frame.length = static_cast<std::uint8_t>(line.dataHex.size() / 2);
    for (std::size_t index = 0; index < frame.length; ++index) {
        frame.data.at(index) = static_cast<std::uint8_t>(
            std::stoul(line.dataHex.substr(index * 2, 2), nullptr, hexBase));
    }
    return frame;
}

double decodedValue(const DecodeResult& result, const std::string& name, bool& found) {
    for (const auto& decoded : result.values) {
        if (decoded.name == name) {
            found = true;
            return decoded.value;
        }
    }
    found = false;
    return 0.0;
}

std::string describe(const OracleLine& line, double ours, const std::string& reason) {
    std::ostringstream text;
    text << std::setprecision(17) << "frame " << line.frameNumber << " id " << line.identifier
         << (line.extended ? " extended" : "") << " data " << line.dataHex << " signal "
         << line.signalName << ": cantools " << line.value << ", DbcDecoder " << ours
         << ", difference " << std::fabs(ours - line.value) << " (" << reason << ")";
    return text.str();
}

struct Comparison {
    std::size_t frameCount = 0;
    std::set<std::string> signalsSeen;
    std::vector<std::string> mismatches;
};

// Decodes each oracle frame once and compares every value the oracle gave for it.
Comparison compareWithOracle(const std::vector<OracleLine>& lines, DbcDecoder& decoder) {
    Comparison comparison;
    std::map<std::size_t, DecodeResult> decodedFrames;
    for (const OracleLine& line : lines) {
        auto entry = decodedFrames.find(line.frameNumber);
        if (entry == decodedFrames.end()) {
            entry = decodedFrames.emplace(line.frameNumber, decoder.decode(frameOf(line))).first;
        }
        comparison.signalsSeen.insert(line.signalName);
        bool found = false;
        const double ours = decodedValue(entry->second, line.signalName, found);
        if (entry->second.kind != DecodeKind::Decoded || !found) {
            comparison.mismatches.push_back(describe(line, ours, "not decoded"));
        } else if (std::fabs(ours - line.value) > tolerance) {
            comparison.mismatches.push_back(describe(line, ours, "above 1e-6"));
        }
    }
    comparison.frameCount = decodedFrames.size();
    return comparison;
}

TEST(DbcOracleTest, EveryValueOfTenThousandFramesAgreesWithCantools) {
    const char* const oraclePath = std::getenv("LEXUS_HEAD_UNIT_DBC_ORACLE_CSV");
    ASSERT_NE(oraclePath, nullptr) << "LEXUS_HEAD_UNIT_DBC_ORACLE_CSV is not set";
    const std::vector<OracleLine> lines = readOracle(oraclePath);
    ASSERT_FALSE(lines.empty()) << "no oracle output in " << oraclePath;
    DbcDecoder decoder(DbcDatabase::loadFromFile(LEXUS_HEAD_UNIT_DBC_PATH));
    ASSERT_TRUE(decoder.database().errors().empty());

    const Comparison comparison = compareWithOracle(lines, decoder);
    for (const std::string& mismatch : comparison.mismatches) {
        std::cout << "MISMATCH " << mismatch << "\n";
    }
    std::cout << "dbc oracle: " << comparison.frameCount << " frames, " << lines.size()
              << " values compared, " << comparison.signalsSeen.size() << " distinct signals, "
              << comparison.mismatches.size() << " mismatches\n";
    EXPECT_GE(comparison.frameCount, minimumFrameCount);
    EXPECT_EQ(comparison.signalsSeen.size(), decoder.database().signalCount());
    EXPECT_EQ(comparison.mismatches.size(), 0U);
}

} // namespace

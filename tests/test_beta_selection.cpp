// =============================================================================
// BetaSelection — pure value-type unit tests (no CTImage, no geometry library).
// =============================================================================
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "models/BetaSelection.h"
#include "models/ShellStatistics.h"

namespace {

BetaSelection::SliceRecord record(int slice, double beta, bool judgeable, bool changepoint, int cut,
                                  double b2Contrast, int nShells) {
    BetaSelection::SliceRecord r;
    r.sliceIndex = slice;
    r.beta = beta;
    r.judgeable = judgeable;
    r.changepointFound = changepoint;
    r.cut = cut;
    r.b2Contrast = b2Contrast;
    r.nShells = nShells;
    return r;
}

const double kNaN = std::numeric_limits<double>::quiet_NaN();

}  // namespace

TEST(BetaSelection, B2SeparationContrastIsLogRatioOfSquaredDistances) {
    // index:                  0     1    2    3    4
    std::vector<double> d = {-1.0, -1.0, 4.0, 2.0, 1.0};
    // cut = 2: log(4^2 / 2^2) = log 4
    EXPECT_DOUBLE_EQ(BetaSelection::b2SeparationContrast(d, 2, 2), std::log(4.0));
    // cut = 3: log(2^2 / 1^2) = log 4
    EXPECT_DOUBLE_EQ(BetaSelection::b2SeparationContrast(d, 3, 2), std::log(4.0));
}

TEST(BetaSelection, B2SeparationContrastUndefinedCases) {
    std::vector<double> d = {-1.0, -1.0, 4.0, 2.0, 0.0};
    EXPECT_TRUE(std::isnan(BetaSelection::b2SeparationContrast(d, 1, 2)));  // cut < 2
    EXPECT_TRUE(std::isnan(BetaSelection::b2SeparationContrast(d, 4, 2)));  // no shell after cut
    EXPECT_TRUE(std::isnan(BetaSelection::b2SeparationContrast(d, 3, 2)));  // d2[cut + 1] == 0
    EXPECT_TRUE(std::isnan(BetaSelection::b2SeparationContrast(d, 2, 3)));  // cut not a fit shell
    std::vector<double> unscored = {-1.0, -1.0, 4.0, -1.0, 1.0};
    EXPECT_TRUE(std::isnan(BetaSelection::b2SeparationContrast(unscored, 2, 2)));
}

TEST(BetaSelection, MedianAndPercentile) {
    EXPECT_TRUE(std::isnan(BetaSelection::median({})));
    EXPECT_DOUBLE_EQ(BetaSelection::median({3.0, 1.0, 2.0}), 2.0);
    EXPECT_DOUBLE_EQ(BetaSelection::median({4.0, 1.0, 3.0, 2.0}), 2.5);

    EXPECT_TRUE(std::isnan(BetaSelection::percentile({}, 50.0)));
    EXPECT_DOUBLE_EQ(BetaSelection::percentile({10.0, 20.0, 30.0, 40.0, 50.0}, 0.0), 10.0);
    EXPECT_DOUBLE_EQ(BetaSelection::percentile({10.0, 20.0, 30.0, 40.0, 50.0}, 100.0), 50.0);
    EXPECT_DOUBLE_EQ(BetaSelection::percentile({10.0, 20.0, 30.0, 40.0, 50.0}, 50.0), 30.0);
    // position 0.9 * 4 = 3.6 -> 40 + 0.6 * 10
    EXPECT_NEAR(BetaSelection::percentile({50.0, 40.0, 30.0, 20.0, 10.0}, 90.0), 46.0, 1e-12);
}

TEST(BetaSelection, RuleNamesRoundTrip) {
    ASSERT_EQ(BetaSelection::allRules().size(), 12u);
    for (BetaSelection::Rule rule : BetaSelection::allRules()) {
        BetaSelection::Rule parsed = BetaSelection::Rule::A1;
        ASSERT_TRUE(BetaSelection::parseRule(BetaSelection::ruleName(rule), parsed))
            << BetaSelection::ruleName(rule);
        EXPECT_EQ(parsed, rule);
    }
    BetaSelection::Rule parsed = BetaSelection::Rule::A1;
    EXPECT_TRUE(BetaSelection::parseRule("b2", parsed));
    EXPECT_EQ(parsed, BetaSelection::Rule::B2);
    EXPECT_TRUE(BetaSelection::parseRule("c3a", parsed));
    EXPECT_EQ(parsed, BetaSelection::Rule::C3a);
    EXPECT_FALSE(BetaSelection::parseRule("Z9", parsed));
}

TEST(BetaSelection, SetParamByName) {
    BetaSelection::Params params;
    EXPECT_TRUE(BetaSelection::setParam(params, "c1_percentile", 75.0));
    EXPECT_DOUBLE_EQ(params.c1Percentile, 75.0);
    EXPECT_TRUE(BetaSelection::setParam(params, "a1_threshold", 0.5));
    EXPECT_DOUBLE_EQ(params.a1Threshold, 0.5);
    EXPECT_FALSE(BetaSelection::setParam(params, "no_such_parameter", 1.0));
    EXPECT_EQ(BetaSelection::listParams(params).size(), 9u);
}

TEST(BetaSelection, B2SelectsLargestMedianContrast) {
    std::vector<BetaSelection::SliceRecord> records = {
        record(0, 0.60, true, true, 2, 1.0, 12), record(1, 0.60, true, true, 2, 2.0, 12),
        record(2, 0.60, true, true, 2, 3.0, 12), record(0, 0.65, true, true, 2, 4.0, 12),
        record(1, 0.65, true, true, 2, 5.0, 12), record(2, 0.65, true, true, 1, kNaN, 12),
        record(0, 0.70, true, true, 2, 0.5, 12), record(1, 0.70, true, true, 2, 0.5, 12),
        record(2, 0.70, true, true, 2, 0.5, 12),
    };
    BetaSelection::Query query;
    query.rule = BetaSelection::Rule::B2;
    const BetaSelection::Selection sel = BetaSelection::select(records, query);

    ASSERT_TRUE(sel.selected);
    EXPECT_DOUBLE_EQ(sel.beta, 0.65);
    EXPECT_DOUBLE_EQ(sel.scoreAtSelected, 4.5);  // median of {4, 5}; the NaN is dropped
    ASSERT_EQ(sel.table.size(), 3u);
    EXPECT_DOUBLE_EQ(sel.table[0].score, 2.0);
    EXPECT_EQ(sel.table[1].nUsed, 2);
    EXPECT_EQ(sel.table[1].nValid, 3);
    EXPECT_EQ(sel.table[1].nCutGe2, 2);
}

TEST(BetaSelection, TieGoesToSmallerBeta) {
    std::vector<BetaSelection::SliceRecord> records = {
        record(0, 0.70, true, true, 2, 2.0, 12),
        record(0, 0.60, true, true, 2, 2.0, 12),
        record(0, 0.65, true, true, 2, 1.0, 12),
    };
    BetaSelection::Query query;
    query.rule = BetaSelection::Rule::B2;
    const BetaSelection::Selection sel = BetaSelection::select(records, query);
    ASSERT_TRUE(sel.selected);
    EXPECT_DOUBLE_EQ(sel.beta, 0.60);
}

TEST(BetaSelection, B4SelectsSmallestShareWithoutChangepoint) {
    std::vector<BetaSelection::SliceRecord> records = {
        // beta 0.60: 2 of 4 without a changepoint (one of them not judgeable)
        record(0, 0.60, true, true, 2, 1.0, 12),
        record(1, 0.60, true, false, 1, kNaN, 12),
        record(2, 0.60, false, false, 0, kNaN, 3),
        record(3, 0.60, true, true, 2, 1.0, 12),
        // beta 0.65: 1 of 4
        record(0, 0.65, true, true, 2, 1.0, 12),
        record(1, 0.65, true, true, 2, 1.0, 12),
        record(2, 0.65, true, false, 1, kNaN, 12),
        record(3, 0.65, true, true, 2, 1.0, 12),
    };
    BetaSelection::Query query;
    query.rule = BetaSelection::Rule::B4;
    const BetaSelection::Selection sel = BetaSelection::select(records, query);
    ASSERT_TRUE(sel.selected);
    EXPECT_DOUBLE_EQ(sel.beta, 0.65);
    EXPECT_DOUBLE_EQ(sel.scoreAtSelected, 0.25);
    EXPECT_DOUBLE_EQ(sel.table[0].score, 0.5);
    EXPECT_DOUBLE_EQ(sel.table[0].judgeableShare, 0.75);
}

TEST(BetaSelection, ValidSliceAndCandidateRestrictions) {
    std::vector<BetaSelection::SliceRecord> records = {
        record(0, 0.60, true, true, 2, 9.0, 12), record(1, 0.60, true, true, 2, 1.0, 12),
        record(0, 0.65, true, true, 2, 2.0, 12), record(1, 0.65, true, true, 2, 2.0, 12),
        record(0, 0.70, true, true, 2, 8.0, 12), record(1, 0.70, true, true, 2, 8.0, 12),
    };
    BetaSelection::Query query;
    query.rule = BetaSelection::Rule::B2;
    query.restrictSlices = true;
    query.validSlices = {1};  // slice 0 excluded: 0.60 scores 1, 0.65 scores 2
    query.restrictBetas = true;
    query.betaMin = 0.60;
    query.betaMax = 0.65;  // 0.70 is not a candidate
    const BetaSelection::Selection sel = BetaSelection::select(records, query);
    ASSERT_TRUE(sel.selected);
    EXPECT_DOUBLE_EQ(sel.beta, 0.65);
    EXPECT_EQ(sel.table.size(), 2u);
}

TEST(BetaSelection, C4SelectsShellCountPlateauEntry) {
    // Median shell count per beta: 10, 20, 21, 21 -> plateau starts at 0.65.
    std::vector<BetaSelection::SliceRecord> records = {
        record(0, 0.60, true, true, 2, 1.0, 10),
        record(0, 0.65, true, true, 2, 1.0, 20),
        record(0, 0.70, true, true, 2, 1.0, 21),
        record(0, 0.75, true, true, 2, 1.0, 21),
    };
    BetaSelection::Query query;
    query.rule = BetaSelection::Rule::C4;
    const BetaSelection::Selection sel = BetaSelection::select(records, query);
    ASSERT_TRUE(sel.selected);
    EXPECT_DOUBLE_EQ(sel.beta, 0.65);
}

TEST(BetaSelection, NotDecidableCasesCarryAReason) {
    BetaSelection::Query query;
    query.rule = BetaSelection::Rule::B2;
    BetaSelection::Selection sel = BetaSelection::select({}, query);
    EXPECT_FALSE(sel.selected);
    EXPECT_FALSE(sel.reason.empty());

    // A1 needs per-shell data for every judgeable slice.
    std::vector<BetaSelection::SliceRecord> records = {
        record(0, 0.60, true, true, 2, 1.0, 12),
        record(0, 0.65, true, true, 2, 1.0, 12),
    };
    query.rule = BetaSelection::Rule::A1;
    sel = BetaSelection::select(records, query);
    EXPECT_FALSE(sel.selected);
    EXPECT_FALSE(sel.reason.empty());

    // C4 needs two candidates.
    query.rule = BetaSelection::Rule::C4;
    sel = BetaSelection::select({record(0, 0.60, true, true, 2, 1.0, 12)}, query);
    EXPECT_FALSE(sel.selected);
    EXPECT_FALSE(sel.reason.empty());
}

TEST(BetaSelection, FillFromShellsCollectsSkinVertexCountsAndBicTerms) {
    // Shells 0..6; fit shells are 2..6 with a clear change after shell 3.
    std::vector<BetaSelection::ShellRow> shells(7);
    const double d2[7] = {kNaN, kNaN, 30.0, 29.0, 1.0, 1.2, 0.8};
    for (int i = 0; i < 7; ++i) {
        shells[i].shellIndex = i;
        shells[i].mu = 1.0 + i;
        shells[i].sigma = 0.1 * (i % 3) + 0.05 * i;
        shells[i].nVertices = 100 - 10 * i;
        shells[i].d2 = d2[i];
    }
    BetaSelection::SliceRecord rec = record(0, 0.65, true, true, 3, kNaN, 7);
    BetaSelection::fillFromShells(rec, shells, 2);

    EXPECT_TRUE(rec.hasShellData);
    EXPECT_EQ(rec.skinVertexCounts, (std::vector<int>{100, 90, 80, 70}));
    const std::vector<double> fit = {30.0, 29.0, 1.0, 1.2, 0.8};
    EXPECT_DOUBLE_EQ(rec.dBic, ShellStatistics::bicChangepointTerms(fit).deltaBic());
    EXPECT_TRUE(std::isfinite(rec.bodyLogDet));  // three body shells (4, 5, 6)

    BetaSelection::SliceRecord notJudgeable = record(0, 0.65, false, false, 0, kNaN, 7);
    BetaSelection::fillFromShells(notJudgeable, shells, 2);
    EXPECT_TRUE(notJudgeable.hasShellData);
    EXPECT_TRUE(notJudgeable.skinVertexCounts.empty());
}

TEST(ShellStatisticsBic, TermsAgreeWithChangepoint) {
    const std::vector<double> stepped = {30.0, 29.0, 31.0, 1.0, 1.2, 0.8, 1.1};
    const ShellStatistics::BicTerms terms = ShellStatistics::bicChangepointTerms(stepped);
    ASSERT_TRUE(terms.defined);
    EXPECT_EQ(terms.changepoint, ShellStatistics::bicChangepoint(stepped));
    EXPECT_GE(terms.changepoint, 0);
    EXPECT_GT(terms.deltaBic(), 0.0);

    const ShellStatistics::BicTerms tooShort = ShellStatistics::bicChangepointTerms({1.0, 2.0});
    EXPECT_FALSE(tooShort.defined);
    EXPECT_EQ(tooShort.changepoint, -1);
    EXPECT_TRUE(std::isnan(tooShort.deltaBic()));
}

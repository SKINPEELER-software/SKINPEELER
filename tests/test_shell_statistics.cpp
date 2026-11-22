// =============================================================================
// Stage 2 tests — ShellStatistics (S1-extracted pure cutting-statistics math)
//
// All inputs are synthetic (mu, sigma) values; no CTImage/DICOM/Voronoi
// dependency. Expected values for the Mahalanobis-based tests were derived
// independently (hand calculation / numpy cross-check) rather than copied
// from the implementation under test.
// =============================================================================
#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

#include "models/ShellStatistics.h"

using ShellStatistics::cutting3SigmaDistance;
using ShellStatistics::cuttingEuclideanDistance;
using ShellStatistics::findDistanceJump;
using ShellStatistics::invert2x2;
using ShellStatistics::mahalanobis2D;

// -----------------------------------------------------------------------------
// invert2x2
// -----------------------------------------------------------------------------
TEST(Invert2x2, DiagonalMatrixKnownInverse) {
    // cov = diag(4, 9) -> inverse = diag(1/4, 1/9)
    double cov[2][2] = {{4.0, 0.0}, {0.0, 9.0}};
    double inv[2][2];
    ASSERT_TRUE(invert2x2(cov, inv));
    EXPECT_NEAR(inv[0][0], 0.25, 1e-9);
    EXPECT_NEAR(inv[0][1], 0.0, 1e-9);
    EXPECT_NEAR(inv[1][0], 0.0, 1e-9);
    EXPECT_NEAR(inv[1][1], 1.0 / 9.0, 1e-9);
}

TEST(Invert2x2, GeneralMatrixKnownInverse) {
    // cov = [[2,1],[1,2]], det=3 -> inverse = [[2/3,-1/3],[-1/3,2/3]]
    double cov[2][2] = {{2.0, 1.0}, {1.0, 2.0}};
    double inv[2][2];
    ASSERT_TRUE(invert2x2(cov, inv));
    EXPECT_NEAR(inv[0][0], 2.0 / 3.0, 1e-9);
    EXPECT_NEAR(inv[0][1], -1.0 / 3.0, 1e-9);
    EXPECT_NEAR(inv[1][0], -1.0 / 3.0, 1e-9);
    EXPECT_NEAR(inv[1][1], 2.0 / 3.0, 1e-9);
}

TEST(Invert2x2, SingularMatrixReturnsFalse) {
    double cov[2][2] = {{1.0, 2.0}, {2.0, 4.0}};  // det == 0
    double inv[2][2] = {{0, 0}, {0, 0}};
    EXPECT_FALSE(invert2x2(cov, inv));
}

// -----------------------------------------------------------------------------
// mahalanobis2D
// -----------------------------------------------------------------------------
TEST(Mahalanobis2D, IdentityCovarianceEqualsEuclidean) {
    double inv[2][2] = {{1.0, 0.0}, {0.0, 1.0}};
    double diff[2] = {3.0, 4.0};
    EXPECT_NEAR(mahalanobis2D(diff, inv), 5.0, 1e-9);  // 3-4-5 triangle
}

TEST(Mahalanobis2D, KnownNumericExample) {
    // cov = diag(4, 9), diff = (2, 3)
    // d^2 = 2^2/4 + 3^2/9 = 1 + 1 = 2  =>  d = sqrt(2)
    double cov[2][2] = {{4.0, 0.0}, {0.0, 9.0}};
    double inv[2][2];
    ASSERT_TRUE(invert2x2(cov, inv));
    double diff[2] = {2.0, 3.0};
    EXPECT_NEAR(mahalanobis2D(diff, inv), std::sqrt(2.0), 1e-9);
}

// -----------------------------------------------------------------------------
// cuttingEuclideanDistance
// -----------------------------------------------------------------------------
TEST(CuttingEuclideanDistance, EmptyPointsReturnsZero) {
    std::vector<std::pair<double, double>> points;
    EXPECT_EQ(cuttingEuclideanDistance(points, 1), 0);
}

TEST(CuttingEuclideanDistance, PicksLargestConsecutiveJump) {
    // Jump between index 1 and 2 (distance ~6.93) dwarfs the other two
    // consecutive jumps (~0.14 each) -> maxIndex == 1.
    std::vector<std::pair<double, double>> points = {
        {0.0, 0.0}, {0.1, 0.1}, {5.0, 5.0}, {5.1, 5.1}};
    EXPECT_EQ(cuttingEuclideanDistance(points, 0), 1);
    EXPECT_EQ(cuttingEuclideanDistance(points, 1), 2);  // offset-adjusted
}

// -----------------------------------------------------------------------------
// findDistanceJump — boundary cases
// -----------------------------------------------------------------------------
TEST(FindDistanceJump, EmptyInputReturnsZero) {
    std::vector<std::pair<double, double>> points;
    EXPECT_EQ(findDistanceJump(points, 1, 2.0), 0);
}

TEST(FindDistanceJump, SingleShellReturnsZero) {
    std::vector<std::pair<double, double>> points = {{1.0, 2.0}};
    EXPECT_EQ(findDistanceJump(points, 1, 2.0), 0);
}

TEST(FindDistanceJump, IdenticalConsecutiveDistancesReturnsOffset) {
    // Every consecutive distance == 1.0 -> dstd == 0 -> threshold == dmean ==
    // 1.0. No distance strictly exceeds the threshold -> falls through to
    // the "no jump found" return (== offset).
    std::vector<std::pair<double, double>> points = {
        {0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {3.0, 0.0}};
    EXPECT_EQ(findDistanceJump(points, 1, 2.0), 1);
    EXPECT_EQ(findDistanceJump(points, 0, 2.0), 0);
}

TEST(FindDistanceJump, ClearJumpIsDetected) {
    // 7 small steps (~0.1414 each) then one large jump (~27.44) -> clearly
    // exceeds mean + 2*stddev. Jump is the 7th consecutive distance (index 6).
    std::vector<std::pair<double, double>> points = {{0.0, 0.0}, {0.1, 0.1},   {0.2, 0.2},
                                                     {0.3, 0.3}, {0.4, 0.4},   {0.5, 0.5},
                                                     {0.6, 0.6}, {20.0, 20.0}, {20.1, 20.1}};
    EXPECT_EQ(findDistanceJump(points, 0, 2.0), 6);
    EXPECT_EQ(findDistanceJump(points, 1, 2.0), 7);
}

// -----------------------------------------------------------------------------
// cutting3SigmaDistance
// -----------------------------------------------------------------------------
TEST(Cutting3SigmaDistance, FewerThanFourPointsNotComputed) {
    std::vector<std::pair<double, double>> points = {{0, 0}, {1, 1}, {2, 2}};
    auto result = cutting3SigmaDistance(points, 1, 3.0, 2.0);
    EXPECT_FALSE(result.computed);
}

TEST(Cutting3SigmaDistance, SingularCovarianceNotComputed) {
    // All points collinear with identical (mu, sigma) deltas -> the
    // covariance matrix is singular (perfectly correlated mu/sigma).
    std::vector<std::pair<double, double>> points = {
        {0.0, 0.0}, {1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}, {4.0, 4.0}};
    auto result = cutting3SigmaDistance(points, 1, 3.0, 2.0);
    EXPECT_FALSE(result.computed);
}

// Well-separated: one outlier (far "transition" shell) followed by a tight
// plateau cluster of 12 "stable body" shells. Expected result (independently
// cross-checked with numpy's np.cov/linalg.inv): the outlier (index 0) falls
// outside the 3-sigma ellipse, the first plateau point (index 1) is the
// first "inside" point -> cuttingIndex = (1-1) + offset = offset.
TEST(Cutting3SigmaDistance, WellSeparatedShellsFlagsOutlierAndCutsAfterIt) {
    std::vector<std::pair<double, double>> points = {
        {0.00, 0.00},  // outlier / transition shell
        {5.01, 5.02}, {4.99, 4.98}, {5.02, 4.99}, {4.98, 5.01}, {5.00, 5.00}, {5.01, 4.99},
        {4.99, 5.02}, {5.02, 5.01}, {4.98, 4.99}, {5.00, 5.02}, {5.01, 5.00}, {4.99, 4.98},
    };
    const int offset = 1;
    auto result = cutting3SigmaDistance(points, offset, 3.0, 2.0);

    ASSERT_TRUE(result.computed);
    ASSERT_EQ(result.inside.size(), points.size());
    EXPECT_FALSE(result.inside[0]);          // outlier outside the ellipse
    EXPECT_TRUE(result.inside[1]);           // first plateau point inside
    EXPECT_EQ(result.cuttingIndex, offset);  // (1-1)+offset
}

// Poorly separated: no transition shell, just jitter around a single
// plateau. Every point falls inside the 3-sigma ellipse (including index 0),
// which triggers the distance-jump fallback path. With near-uniform
// consecutive distances, no jump exceeds the threshold -> falls back to
// `offset`.
TEST(Cutting3SigmaDistance, PoorlySeparatedShellsFallsBackToDistanceJump) {
    std::vector<std::pair<double, double>> points = {
        {5.00, 5.00}, {5.01, 4.99}, {4.99, 5.01}, {5.02, 5.00},
        {4.98, 5.00}, {5.00, 5.02}, {5.00, 4.98}, {5.01, 5.01},
    };
    const int offset = 1;
    auto result = cutting3SigmaDistance(points, offset, 3.0, 2.0);

    ASSERT_TRUE(result.computed);
    EXPECT_TRUE(result.inside[0]);
    EXPECT_EQ(result.cuttingIndex, offset);
}

// =============================================================================
// Mahalanobis + BIC changepoint cut (cuttingMahalBic). Expected values were
// computed with the Python harness (cell_mahal_ic / _changepoint_ic in
// experiments/C_comparison/exp_odm_selection_criteria/run_experiment1_2D.py)
// on the same (mu, sigma) inputs. points = S1..Sm, offset = 1.
// =============================================================================
using ShellStatistics::bicChangepoint;
using ShellStatistics::cuttingMahalBic;
using ShellStatistics::cutFromThreshold;

TEST(MahalBic, CutFromThresholdSemantics) {
    // first shell with d <= tau minus one, minimum 1, none -> last shell index
    EXPECT_EQ(cutFromThreshold({5.0, 4.0, 1.0, 0.5}, 2, 2.0), 3);  // first inside = shell 4
    EXPECT_EQ(cutFromThreshold({1.0, 4.0}, 2, 2.0), 1);            // first inside = shell 2 -> max(1,1)
    EXPECT_EQ(cutFromThreshold({5.0, 4.0, 3.0}, 2, 2.0), 4);       // none inside -> last shell
}

TEST(MahalBic, TwoRegime) {
    std::vector<std::pair<double, double>> pts = {
        {2000.1709638362659, 906.7987377015498},
        {2066.1236053929297, 847.4484646160616},
        {2118.5101524444676, 797.3630790348328},
        {2182.8486317878596, 749.7196778047719},
        {2243.7344280812827, 690.7633760051294},
        {2307.8327438734977, 649.517839199222},
        {1005.4430276261932, 149.31716833011586},
        {996.9672114634012, 152.31555079298792},
        {1006.5961082202409, 148.98735064653275},
        {998.7777105714384, 153.4284930540463},
        {993.0372748644227, 142.428082481343},
        {1003.1598549019963, 146.6471708815606},
        {984.6372752790558, 145.9297316802732},
        {996.259219528858, 144.0339876128387},
        {988.0602889274958, 150.18318913472402},
        {1007.177994053822, 148.8343396101977}};
    auto r = cuttingMahalBic(pts, 1);
    ASSERT_TRUE(r.computed);
    EXPECT_EQ(r.cuttingIndex, 2);  // Python cell 2
    EXPECT_TRUE(r.changepointFound);
    EXPECT_NEAR(r.tau, 1.9734352473766765, 1e-9);
}

TEST(MahalBic, DriftThenFlat) {
    std::vector<std::pair<double, double>> pts = {
        {3000.0, 800.0},
        {2800.0, 760.0},
        {2600.0, 720.0},
        {2400.0, 680.0},
        {2200.0, 640.0},
        {1992.5640397049115, 302.30996285248744},
        {2007.1723580719438, 298.19993640906915},
        {2005.4466780792088, 306.2572528594977},
        {1997.9304356379168, 295.1189067481106},
        {2003.4765059851552, 301.48527444577707},
        {2010.988127684144, 292.2925153271679},
        {1993.3838706964445, 294.97099823570596},
        {1982.6598515376716, 300.75860733118196},
        {2005.2780421249552, 295.56725981114516},
        {2013.8564707449616, 304.93154601996264},
        {2006.2737647883553, 302.41024254864584},
        {2009.5566956444864, 292.0081209627414}};
    auto r = cuttingMahalBic(pts, 1);
    ASSERT_TRUE(r.computed);
    EXPECT_EQ(r.cuttingIndex, 2);  // Python cell 2
    EXPECT_TRUE(r.changepointFound);
    EXPECT_NEAR(r.tau, 2.2077200546521256, 1e-9);
}

TEST(MahalBic, SingleGaussian) {
    std::vector<std::pair<double, double>> pts = {
        {1530.6964829124931, 418.08330500600346},
        {1411.6140711428513, 410.4109030616314},
        {1487.4789326645016, 423.445680881851},
        {1478.0468906181165, 399.452774270527},
        {1517.1425758658827, 373.7121493376738},
        {1529.9298324090191, 396.85110434429},
        {1524.6241311839622, 384.34487480989634},
        {1554.3100771638758, 418.1560593528842},
        {1491.0987487640332, 418.9587147128083},
        {1562.9877580679313, 453.73526540493964},
        {1421.321181477989, 426.4939543486756},
        {1523.253425425669, 397.1841765944097}};
    auto r = cuttingMahalBic(pts, 1);
    ASSERT_TRUE(r.computed);
    EXPECT_EQ(r.cuttingIndex, 2);  // Python cell 2
    EXPECT_TRUE(r.changepointFound);
    EXPECT_NEAR(r.tau, 1.3673746092425147, 1e-9);
}

TEST(MahalBic, FarSkin) {
    std::vector<std::pair<double, double>> pts = {
        {3979.8667013004588, 1512.5718861347314},
        {4274.765240131109, 1605.6694546573474},
        {4626.037359924054, 1684.0033071194853},
        {995.462232392751, 189.5262654598696},
        {1003.6608116205385, 212.11500104539724},
        {1030.353340437583, 185.7750844569319},
        {991.3757649141841, 205.6283607466473},
        {1023.6905897813983, 203.36968515422637},
        {988.8077200361394, 202.3770520770995},
        {999.7507119852875, 198.37007422089465},
        {988.9829335217887, 203.09807804754405},
        {1004.6181943954872, 199.2561269793892},
        {996.67468090449, 189.72066743568263},
        {992.7073649928785, 209.65159737077315},
        {997.1416261470096, 188.482421465295},
        {1020.0166421042697, 204.24212326786335},
        {1031.6213054293796, 200.50009735513376}};
    auto r = cuttingMahalBic(pts, 1);
    ASSERT_TRUE(r.computed);
    EXPECT_EQ(r.cuttingIndex, 3);  // Python cell 2
    EXPECT_TRUE(r.changepointFound);
    EXPECT_NEAR(r.tau, 2.4070670576003814, 1e-9);
}

TEST(MahalBic, MinFourNoChangepoint) {
    std::vector<std::pair<double, double>> pts = {
        {2500.0, 900.0},
        {2100.0, 700.0},
        {1500.0, 420.0},
        {1480.0, 415.0}};
    auto r = cuttingMahalBic(pts, 1);
    ASSERT_TRUE(r.computed);
    EXPECT_EQ(r.cuttingIndex, 4);  // Python cell 2
    EXPECT_FALSE(r.changepointFound);
    EXPECT_TRUE(std::isnan(r.tau));
}

TEST(MahalBic, FiveShells) {
    std::vector<std::pair<double, double>> pts = {
        {2500.0, 900.0},
        {2100.0, 700.0},
        {1500.0, 420.0},
        {1480.0, 415.0},
        {1510.0, 410.0}};
    auto r = cuttingMahalBic(pts, 1);
    ASSERT_TRUE(r.computed);
    EXPECT_EQ(r.cuttingIndex, 2);  // Python cell 2
    EXPECT_TRUE(r.changepointFound);
    EXPECT_NEAR(r.tau, 1.1507085010209233, 1e-9);
}

TEST(MahalBic, TooFewNotComputed) {
    std::vector<std::pair<double, double>> pts = {
        {2500.0, 900.0},
        {2100.0, 700.0},
        {1500.0, 420.0}};
    EXPECT_FALSE(cuttingMahalBic(pts, 1).computed);
}

TEST(MahalBic, ChangepointStep) {
    EXPECT_EQ(bicChangepoint({9.0, 8.5, 9.2, 8.8, 1.0, 1.1, 0.9, 1.2, 1.0, 0.8}), 4);  // Python _changepoint_ic (-1 = None)
}

TEST(MahalBic, ChangepointFlat) {
    EXPECT_EQ(bicChangepoint({1.0, 1.1, 0.9, 1.05, 0.95, 1.0, 1.02, 0.98}), 7);  // Python _changepoint_ic (-1 = None)
}

TEST(MahalBic, ChangepointShort) {
    EXPECT_EQ(bicChangepoint({1.0, 5.0}), -1);  // Python _changepoint_ic (-1 = None)
}

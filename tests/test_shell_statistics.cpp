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

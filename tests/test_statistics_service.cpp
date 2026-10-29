// =============================================================================
// Stage 1 tests — StatisticsService::computeInterShellDistances
// (pure given a vector<pair<double,double>>; no CTImage instance needed)
// =============================================================================
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "services/StatisticsService.h"

TEST(ComputeInterShellDistances, FewerThanTwoPointsInvalid) {
    std::vector<std::pair<double, double>> empty;
    auto r0 = StatisticsService::computeInterShellDistances(empty);
    EXPECT_FALSE(r0.valid);

    std::vector<std::pair<double, double>> single = {{1.0, 2.0}};
    auto r1 = StatisticsService::computeInterShellDistances(single);
    EXPECT_FALSE(r1.valid);
}

TEST(ComputeInterShellDistances, KnownDistancesMeanMaxMin) {
    // (0,0) -> (3,4): distance 5 (3-4-5 triangle)
    // (3,4) -> (3,4): distance 0
    std::vector<std::pair<double, double>> points = {{0.0, 0.0}, {3.0, 4.0}, {3.0, 4.0}};
    auto result = StatisticsService::computeInterShellDistances(points);

    ASSERT_TRUE(result.valid);
    ASSERT_EQ(result.distances.size(), 2u);
    EXPECT_NEAR(result.distances[0], 5.0, 1e-9);
    EXPECT_NEAR(result.distances[1], 0.0, 1e-9);
    EXPECT_NEAR(result.mean, 2.5, 1e-9);
    EXPECT_NEAR(result.max, 5.0, 1e-9);
    EXPECT_NEAR(result.min, 0.0, 1e-9);
}

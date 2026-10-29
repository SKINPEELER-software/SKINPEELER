// =============================================================================
// Stage 1 tests — PeelingService::computeSectionIndices / interpolateThickness
// (pure static functions; no CTImage instance needed)
// =============================================================================
#include <gtest/gtest.h>
#include <vector>

#include "services/PeelingService.h"

// -----------------------------------------------------------------------------
// computeSectionIndices
// -----------------------------------------------------------------------------
TEST(ComputeSectionIndices, EvenlySpacedIndices) {
    auto indices = PeelingService::computeSectionIndices(100, 5);
    std::vector<int> expected = {0, 20, 40, 60, 80};
    EXPECT_EQ(indices, expected);
}

TEST(ComputeSectionIndices, ClampsNumSectionsToTotalImages) {
    // numSections (20) > totalImages (10) -> clamped to 10, interval 1
    auto indices = PeelingService::computeSectionIndices(10, 20);
    std::vector<int> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    EXPECT_EQ(indices, expected);
}

// -----------------------------------------------------------------------------
// interpolateThickness
// -----------------------------------------------------------------------------
TEST(InterpolateThickness, LinearBetweenTwoKeySections) {
    // sections=[0,9], thicknesses=[2,8] over 10 images
    auto result = PeelingService::interpolateThickness(10, {0, 9}, {2, 8});
    ASSERT_EQ(result.size(), 10u);
    EXPECT_NEAR(result[0], 2.0, 1e-9);
    EXPECT_NEAR(result[9], 8.0, 1e-9);
    // i=5: t = (8-2)/(9-0)*(5-0) + 2 = 6/9*5 + 2 = 5.333...
    EXPECT_NEAR(result[5], 5.0 + 1.0 / 3.0, 1e-6);
}

TEST(InterpolateThickness, BeforeFirstAndAfterLastSectionClamp) {
    // sections=[2,8], thicknesses=[5,9] over 10 images
    auto result = PeelingService::interpolateThickness(10, {2, 8}, {5, 9});
    ASSERT_EQ(result.size(), 10u);
    EXPECT_NEAR(result[0], 5.0, 1e-9);  // before first section -> thicknesses.front()
    EXPECT_NEAR(result[9], 9.0, 1e-9);  // after last section -> thicknesses.back()
    // i=5: t = (9-5)/(8-2)*(5-2) + 5 = 4/6*3 + 5 = 7.0
    EXPECT_NEAR(result[5], 7.0, 1e-9);
}

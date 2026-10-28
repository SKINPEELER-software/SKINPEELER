// =============================================================================
// Stage 1 tests — CTShell::calculateStatistics (pure, no V-IP/DCMTK dependency)
// =============================================================================
#include <gtest/gtest.h>

#include <cmath>

#include "models/CTShell.h"

TEST(CTShellStatistics, EmptyShellHasNoStatistics) {
    CTShell shell;
    shell.calculateStatistics();
    EXPECT_FALSE(shell.hasStatistics());
}

TEST(CTShellStatistics, SingleVertexMeanEqualsDiameterZeroSpread) {
    CTShell shell;
    shell.addVertex(0.0, 0.0, 2.5);  // radius = 2.5
    shell.calculateStatistics();
    ASSERT_TRUE(shell.hasStatistics());
    EXPECT_NEAR(shell.mu(), 5.0, 1e-9);     // diameter = 2*radius
    EXPECT_NEAR(shell.sigma(), 0.0, 1e-9);  // single point -> zero spread
}

TEST(CTShellStatistics, KnownRadiiPopulationStatistics) {
    // radii = {1, 2, 3} -> mean radius = 2 -> mu (mean diameter) = 4
    // population variance of radius = ((1-2)^2+(2-2)^2+(3-2)^2)/3 = 2/3
    // population std of radius = sqrt(2/3) -> sigma (std of diameter) = 2*sqrt(2/3)
    CTShell shell;
    shell.addVertex(0.0, 0.0, 1.0);
    shell.addVertex(1.0, 1.0, 2.0);
    shell.addVertex(2.0, 2.0, 3.0);
    shell.calculateStatistics();

    ASSERT_TRUE(shell.hasStatistics());
    EXPECT_NEAR(shell.mu(), 4.0, 1e-9);
    EXPECT_NEAR(shell.sigma(), 2.0 * std::sqrt(2.0 / 3.0), 1e-9);
}

TEST(CTShellStatistics, AddVertexInvalidatesCachedStatistics) {
    CTShell shell;
    shell.addVertex(0.0, 0.0, 1.0);
    shell.calculateStatistics();
    ASSERT_TRUE(shell.hasStatistics());

    shell.addVertex(0.0, 0.0, 5.0);  // should invalidate mu/sigma until recomputed
    EXPECT_FALSE(shell.hasStatistics());

    shell.calculateStatistics();
    EXPECT_TRUE(shell.hasStatistics());
}

TEST(CTShellStatistics, ClearResetsVerticesAndStatistics) {
    CTShell shell;
    shell.addVertex(0.0, 0.0, 1.0);
    shell.calculateStatistics();
    shell.clear();
    EXPECT_TRUE(shell.isEmpty());
    EXPECT_FALSE(shell.hasStatistics());
}

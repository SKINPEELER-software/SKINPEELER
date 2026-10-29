// =============================================================================
// Stage 1 tests — BedDetector::fitPolynomial / maxCurvature / checkSagAndChord
// (public static pure functions; no VD/DICOM dependency)
// =============================================================================
#include <gtest/gtest.h>

#include "models/BedDetector.h"

// -----------------------------------------------------------------------------
// fitPolynomial
// x values span exactly [-1, 1], so the internal normalisation
// (xn = 2*(x-xmin)/xrange - 1) is the identity and fitted coefficients can be
// compared directly against the original-domain polynomial.
// -----------------------------------------------------------------------------
TEST(BedDetectorFitPolynomial, LinearFitRecoversExactCoefficients) {
    // y = 3 + 2x over x in [-1, 0, 1]
    std::vector<double> x = {-1.0, 0.0, 1.0};
    std::vector<double> y = {1.0, 3.0, 5.0};
    auto coeffs = BedDetector::fitPolynomial(x, y, 1);
    ASSERT_EQ(coeffs.size(), 2u);
    EXPECT_NEAR(coeffs[0], 3.0, 1e-6);
    EXPECT_NEAR(coeffs[1], 2.0, 1e-6);
}

TEST(BedDetectorFitPolynomial, TooFewPointsReturnsEmpty) {
    std::vector<double> x = {0.0};
    std::vector<double> y = {1.0};
    auto coeffs = BedDetector::fitPolynomial(x, y, 1);  // degree 1 needs >= 2 points
    EXPECT_TRUE(coeffs.empty());
}

TEST(BedDetectorFitPolynomial, DegenerateXRangeReturnsEmpty) {
    std::vector<double> x = {5.0, 5.0, 5.0};  // xrange == 0
    std::vector<double> y = {1.0, 2.0, 3.0};
    auto coeffs = BedDetector::fitPolynomial(x, y, 1);
    EXPECT_TRUE(coeffs.empty());
}

// -----------------------------------------------------------------------------
// maxCurvature
// coeffs for y = x^2 in the normalised domain, x0/x1 = [-1, 1] (scale = 1).
// Analytic max curvature at x=0 is exactly 2; discrete sampling (n=50)
// approaches but does not exactly hit x=0, so a small tolerance is used.
// Expected value cross-checked independently: ~1.995.
// -----------------------------------------------------------------------------
TEST(BedDetectorMaxCurvature, QuadraticKnownPeakCurvature) {
    std::vector<double> coeffs = {0.0, 0.0, 1.0};  // y = x^2
    double maxK = BedDetector::maxCurvature(coeffs, -1.0, 1.0, 50);
    EXPECT_NEAR(maxK, 2.0, 0.01);
}

TEST(BedDetectorMaxCurvature, FlatLineHasZeroCurvature) {
    std::vector<double> coeffs = {1.0, 0.5};  // y = 1 + 0.5x (linear -> zero curvature)
    double maxK = BedDetector::maxCurvature(coeffs, -1.0, 1.0, 50);
    EXPECT_NEAR(maxK, 0.0, 1e-9);
}

TEST(BedDetectorMaxCurvature, EmptyCoeffsReturnsZero) {
    std::vector<double> coeffs;
    EXPECT_EQ(BedDetector::maxCurvature(coeffs, -1.0, 1.0, 50), 0.0);
}

// -----------------------------------------------------------------------------
// checkSagAndChord
// -----------------------------------------------------------------------------
namespace {
BedDetector::BedCandidate makeCandidate(const std::vector<int>& cols,
                                        const std::vector<int>& rows) {
    BedDetector::BedCandidate c{};
    c.top_boundary_cols = cols;
    c.top_boundary_rows = rows;
    return c;
}
}  // namespace

TEST(BedDetectorCheckSagAndChord, SaggingArcPassesBothChecks) {
    // cols 0..100 step 10 (11 points). Center rows (idx 4,5,6 -> col 40,50,60)
    // sag far below edges (idx 0,1,9,10 -> col 0,10,90,100).
    std::vector<int> cols = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    std::vector<int> rows = {0, 1, 2, 3, 10, 10, 10, 3, 2, 1, 0};
    auto candidate = makeCandidate(cols, rows);

    bool ok = BedDetector::checkSagAndChord(candidate, /*min_sag_pixels=*/1.5,
                                            /*chord_tolerance_px=*/1.0);
    EXPECT_TRUE(ok);
    EXPECT_NEAR(candidate.sag_pixels, 9.5, 1e-9);  // center mean(10)-edge mean(0.5)
    EXPECT_TRUE(candidate.below_chord_ok);
}

TEST(BedDetectorCheckSagAndChord, FlatBoundaryFailsSagCheck) {
    std::vector<int> cols = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    std::vector<int> rows(cols.size(), 0);  // perfectly flat -> sag = 0
    auto candidate = makeCandidate(cols, rows);

    bool ok = BedDetector::checkSagAndChord(candidate, /*min_sag_pixels=*/1.5,
                                            /*chord_tolerance_px=*/1.0);
    EXPECT_FALSE(ok);
    EXPECT_NEAR(candidate.sag_pixels, 0.0, 1e-9);
}

TEST(BedDetectorCheckSagAndChord, PointAboveChordFailsChordCheck) {
    // Same sagging shape as the passing case, but one interior point (col 30)
    // juts far ABOVE the (flat, row=0) chord -> below-chord check fails.
    std::vector<int> cols = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    std::vector<int> rows = {0, 1, 2, -5, 10, 10, 10, 3, 2, 1, 0};
    auto candidate = makeCandidate(cols, rows);

    bool ok = BedDetector::checkSagAndChord(candidate, /*min_sag_pixels=*/1.5,
                                            /*chord_tolerance_px=*/1.0);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(candidate.below_chord_ok);
}

TEST(BedDetectorCheckSagAndChord, FewerThanThreePointsFailsTrivially) {
    std::vector<int> cols = {0, 10};
    std::vector<int> rows = {0, 0};
    auto candidate = makeCandidate(cols, rows);

    bool ok = BedDetector::checkSagAndChord(candidate, 1.5, 1.0);
    EXPECT_FALSE(ok);
}

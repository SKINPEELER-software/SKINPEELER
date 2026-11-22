#pragma once

#include <limits>
#include <utility>
#include <vector>

// =============================================================================
// ShellStatistics
//
// Pure-math helpers for shell cutting-point detection. Operates only on plain
// value types ((mu, sigma) pairs, raw double arrays) -- no CTImage, V-IP, or
// DCMTK dependency. This is what lets these functions be unit-tested without
// a DICOM file or a Voronoi diagram.
//
// All functions here are free functions / stateless: given the same input
// they always produce the same output.
// =============================================================================
namespace ShellStatistics {

// -------------------------------------------------------------------------
// invert2x2 — invert a 2x2 covariance matrix.
// Returns false (leaving `inv` unmodified) if the matrix is singular
// (|det| < 1e-10).
// -------------------------------------------------------------------------
bool invert2x2(const double cov[2][2], double inv[2][2]);

// -------------------------------------------------------------------------
// mahalanobis2D — Mahalanobis distance of a 2D offset given the inverse
// covariance matrix.
// -------------------------------------------------------------------------
double mahalanobis2D(const double diff[2], const double inv[2][2]);

// -------------------------------------------------------------------------
// cuttingEuclideanDistance — index (in `points` space, offset-adjusted) of
// the largest consecutive Euclidean jump between (mu, sigma) points.
// Returns `offset` if points is empty.
// -------------------------------------------------------------------------
int cuttingEuclideanDistance(const std::vector<std::pair<double, double>>& points, int offset);

// -------------------------------------------------------------------------
// findDistanceJump — index (offset-adjusted) of the first consecutive
// Euclidean distance that exceeds mean + distanceJumpSigma * stddev.
// Returns `offset` if points.size() < 2 or no distance exceeds the
// threshold.
// -------------------------------------------------------------------------
int findDistanceJump(const std::vector<std::pair<double, double>>& points, int offset,
                     double distanceJumpSigma);

// -------------------------------------------------------------------------
// SigmaCuttingResult — result of the 3-sigma ellipse cutting-point
// computation. `computed == false` means the caller must fall back to
// cuttingEuclideanDistance() itself (points.size() < 4, or singular
// covariance) -- mahalDistances/inside/cuttingIndex are not meaningful in
// that case.
// -------------------------------------------------------------------------
struct SigmaCuttingResult {
    bool computed = false;
    std::vector<double> mahalDistances;  // size == points.size(), meaningful iff computed
    std::vector<bool> inside;            // size == points.size(), meaningful iff computed
    int cuttingIndex = 0;                // offset-adjusted shell index, meaningful iff computed
};

// -------------------------------------------------------------------------
// cutting3SigmaDistance — fit a 2D Gaussian (mu, sigma) over `points`,
// compute per-point Mahalanobis distance, and determine the cutting shell
// index: the first point that falls inside the sigmaThreshold ellipse
// (minus one, offset-adjusted), or a distance-jump fallback if none / only
// the first point is inside.
// -------------------------------------------------------------------------
SigmaCuttingResult cutting3SigmaDistance(const std::vector<std::pair<double, double>>& points,
                                         int offset, double sigmaThreshold,
                                         double distanceJumpSigma);

// -------------------------------------------------------------------------
// invert2x2OrPinv — invert a 2x2 covariance matrix with numpy semantics:
// the plain inverse unless the determinant is exactly zero, in which case
// the Moore-Penrose pseudo-inverse is returned (np.linalg.inv -> pinv
// fallback used by the experiment harness).
// -------------------------------------------------------------------------
void invert2x2OrPinv(const double cov[2][2], double inv[2][2]);

// -------------------------------------------------------------------------
// gaussianLogLik — log-likelihood of x[0..n) under N(mu, sigma2).
// sigma2 <= 0 is clamped to 1e-12.
// -------------------------------------------------------------------------
double gaussianLogLik(const double* x, int n, double mu, double sigma2);

// -------------------------------------------------------------------------
// bicChangepoint — BIC changepoint on the squared-distance sequence d2:
// H0 = single Gaussian (p=2) vs the best two-segment heteroscedastic split
// (p=4), BIC = -2L + p ln m. Returns k_pos (>=1, 0-based position of the
// first inner element) or -1 when m < 3 or H0 is not worse than the best
// split.
// -------------------------------------------------------------------------
int bicChangepoint(const std::vector<double>& d2);

// -------------------------------------------------------------------------
// bicChangepointTerms — the quantities bicChangepoint() decides on.
//   ic0     = BIC of H0 (single Gaussian)
//   bestIc1 = smallest BIC over the two-segment splits, at split bestK
//   changepoint = bicChangepoint()'s result (bestK, or -1 when H0 is not worse)
// defined == false when m < 3 (nothing computed).
// deltaBic() = ic0 - bestIc1 (BIC decisiveness; NaN when !defined).
// -------------------------------------------------------------------------
struct BicTerms {
    bool defined = false;
    double ic0 = std::numeric_limits<double>::quiet_NaN();
    double bestIc1 = std::numeric_limits<double>::infinity();
    int bestK = -1;
    int changepoint = -1;
    double deltaBic() const {
        return defined ? ic0 - bestIc1 : std::numeric_limits<double>::quiet_NaN();
    }
};
BicTerms bicChangepointTerms(const std::vector<double>& d2);

// -------------------------------------------------------------------------
// cutFromThreshold — first element with d <= tau is the first inside shell;
// returns max(shellIndex(first inside) - 1, 1), or the last shell index if
// no element is inside. `firstShellIndex` is the shell index of d[0].
// -------------------------------------------------------------------------
int cutFromThreshold(const std::vector<double>& d, int firstShellIndex, double tau);

// -------------------------------------------------------------------------
// MahalBicResult — result of the Mahalanobis + BIC changepoint cut.
// `computed == false` (fewer than 4 non-air shells) means the caller must
// fall back itself; the other fields are then not meaningful.
// The Gaussian is fitted on points[1..] (S2..Sm; S1 is the skin prior and is
// scored but excluded from the fit).
// -------------------------------------------------------------------------
struct MahalBicResult {
    bool computed = false;
    std::vector<double>
        mahalDistances;        // size == points.size(); points[0] scored with the S2..Sm fit
    std::vector<bool> inside;  // d <= tau; points[0] (S1) forced false (skin prior)
    bool changepointFound = false;
    int changepointPos = -1;                                // k_pos within points[1..]
    double tau = std::numeric_limits<double>::quiet_NaN();  // NaN if no changepoint
    double meanMu = 0.0, meanSigma = 0.0;                   // fit location
    double cov[2][2] = {{0, 0}, {0, 0}};                    // fit sample covariance (N-1)
    int cuttingIndex = 0;                                   // shell index (offset-adjusted)
};

// -------------------------------------------------------------------------
// cuttingMahalBic — the SKINPEELER skin-cut rule (classical Mahalanobis +
// BIC changepoint). `points` = (mu, sigma) of S1..Sm, `offset` = shell index
// of points[0].
// -------------------------------------------------------------------------
MahalBicResult cuttingMahalBic(const std::vector<std::pair<double, double>>& points, int offset);

}  // namespace ShellStatistics

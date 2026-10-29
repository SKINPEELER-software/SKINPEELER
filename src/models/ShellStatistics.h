#pragma once

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

}  // namespace ShellStatistics

#include "BedDetector.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <cstdint>
#include <utility>
#include <vector>

// =============================================================================
// fitPolynomial
// OLS polynomial fit using normal equations: (X^T X) c = X^T y
// Inputs x/y are normalised internally to improve numerical stability.
// Returns coefficients [a0, a1, ..., a_degree] or empty on failure.
// =============================================================================
std::vector<double> BedDetector::fitPolynomial(const std::vector<double>& x,
                                               const std::vector<double>& y, int degree) {
    const int n = static_cast<int>(x.size());
    if (n < degree + 1 || n < 2)
        return {};

    // Normalise x to [-1, 1] for numerical stability
    double xmin = *std::min_element(x.begin(), x.end());
    double xmax = *std::max_element(x.begin(), x.end());
    double xrange = xmax - xmin;
    if (xrange < 1e-10)
        return {};

    const int d = degree + 1;  // number of coefficients

    // Build X^T X  (d x d) and X^T y  (d x 1)
    std::vector<double> XtX(d * d, 0.0);
    std::vector<double> Xty(d, 0.0);

    for (int i = 0; i < n; ++i) {
        double xn = 2.0 * (x[i] - xmin) / xrange - 1.0;  // normalised x in [-1,1]
        // Compute powers [1, xn, xn^2, ..., xn^degree]
        std::vector<double> powers(d);
        powers[0] = 1.0;
        for (int j = 1; j < d; ++j)
            powers[j] = powers[j - 1] * xn;

        for (int r = 0; r < d; ++r) {
            Xty[r] += powers[r] * y[i];
            for (int c = 0; c < d; ++c)
                XtX[r * d + c] += powers[r] * powers[c];
        }
    }

    // Solve using Gaussian elimination with partial pivoting
    // Augmented matrix [XtX | Xty]
    std::vector<double> aug(d * (d + 1));
    for (int r = 0; r < d; ++r) {
        for (int c = 0; c < d; ++c)
            aug[r * (d + 1) + c] = XtX[r * d + c];
        aug[r * (d + 1) + d] = Xty[r];
    }

    for (int col = 0; col < d; ++col) {
        // Partial pivot
        int pivot = col;
        double maxVal = std::abs(aug[col * (d + 1) + col]);
        for (int row = col + 1; row < d; ++row) {
            double v = std::abs(aug[row * (d + 1) + col]);
            if (v > maxVal) {
                maxVal = v;
                pivot = row;
            }
        }
        if (maxVal < 1e-12)
            return {};  // singular

        if (pivot != col) {
            for (int c = 0; c <= d; ++c)
                std::swap(aug[col * (d + 1) + c], aug[pivot * (d + 1) + c]);
        }

        double diag = aug[col * (d + 1) + col];
        for (int row = 0; row < d; ++row) {
            if (row == col)
                continue;
            double factor = aug[row * (d + 1) + col] / diag;
            for (int c = col; c <= d; ++c)
                aug[row * (d + 1) + c] -= factor * aug[col * (d + 1) + c];
        }
    }

    // Extract solution in normalised domain
    std::vector<double> cn(d);
    for (int r = 0; r < d; ++r)
        cn[r] = aug[r * (d + 1) + d] / aug[r * (d + 1) + r];

    // Convert coefficients from normalised x back to original x domain.
    // x_norm = 2*(x - xmin)/xrange - 1  =>  x = xnorm * xrange/2 + xmin + xrange/2
    // For degree 2: y = cn0 + cn1*(x_norm) + cn2*(x_norm)^2
    // Substituting x_norm = (2/xrange)*x - (2*xmin/xrange + 1):
    // This transformation is complex for general degree; return coefficients in
    // normalised domain and adjust evaluation in maxCurvature accordingly.
    // Curvature calculation uses normalised coordinates internally.
    return cn;  // coefficients in normalised x space [-1, 1]
    // NOTE: callers that evaluate the polynomial must also normalise x.
    // maxCurvature() handles this correctly by using [-1, 1] range.
}

// =============================================================================
// maxCurvature
// Compute max absolute curvature in the ORIGINAL pixel domain.
// coeffs are in the normalised domain [-1, 1] from fitPolynomial.
// x0, x1: original pixel column range used to fit the polynomial.
//
// Domain conversion:
//   fitPolynomial normalises x_orig to x_norm = 2*(x_orig - xmin)/xrange - 1
//   where xrange = x1 - x0 (original pixel span).
//   Derivatives transform as:
//     dy/dx_orig  = dy/dx_norm * (dx_norm/dx_orig) = y'_norm  * (2/xrange)
//     d2y/dx2_orig = y''_norm * (2/xrange)^2
//   Curvature in original domain:
//     kappa = |y''_orig| / (1 + y'_orig^2)^1.5
//           = |(2/xrange)^2 * y''_norm| / (1 + (2/xrange * y'_norm)^2)^1.5
//
// Curvature must be computed in the original pixel domain and compared
// against theta_curv = 0.003 [1/px] (also in the original domain). Evaluating
// curvature directly in the normalised [-1,1] space without the scale-factor
// conversion above would produce values roughly 14,000x larger than the
// original-domain curvature for a bed spanning ~250 px, causing every bed
// candidate to be rejected as "too curved".
// =============================================================================
double BedDetector::maxCurvature(const std::vector<double>& coeffs, double x0, double x1,
                                 int n_samples) {
    if (coeffs.empty() || n_samples < 1)
        return 0.0;

    const double xrange = x1 - x0;
    if (xrange < 1e-10)
        return 0.0;

    // Scale factor: derivative in original domain = derivative in norm domain * scale
    const double scale = 2.0 / xrange;

    const int d = static_cast<int>(coeffs.size());
    double maxK = 0.0;

    // Evaluate at n_samples points over the normalised range [-1, 1].
    // fitPolynomial returns coefficients in the normalised domain, so we
    // always sample over [-1, 1].
    for (int i = 0; i < n_samples; ++i) {
        double t = (n_samples == 1) ? 0.5 : static_cast<double>(i) / (n_samples - 1);
        double xn = -1.0 + t * 2.0;

        // Compute y'_norm and y''_norm in the normalised domain.
        // y'_norm  = sum_{k=1}^{d-1}  k          * coeffs[k] * xn^(k-1)
        // y''_norm = sum_{k=2}^{d-1}  k*(k-1)    * coeffs[k] * xn^(k-2)
        double yp_norm = 0.0;
        double ypp_norm = 0.0;

        for (int k = 1; k < d; ++k) {
            // xn^(k-1)
            double xn_km1 = 1.0;
            for (int j = 0; j < k - 1; ++j)
                xn_km1 *= xn;
            yp_norm += static_cast<double>(k) * coeffs[k] * xn_km1;
        }
        for (int k = 2; k < d; ++k) {
            // xn^(k-2)
            double xn_km2 = 1.0;
            for (int j = 0; j < k - 2; ++j)
                xn_km2 *= xn;
            ypp_norm += static_cast<double>(k) * static_cast<double>(k - 1) * coeffs[k] * xn_km2;
        }

        // Convert to original pixel domain.
        double yp_orig = yp_norm * scale;
        double ypp_orig = ypp_norm * scale * scale;

        double denom = 1.0 + yp_orig * yp_orig;
        double kappa = std::abs(ypp_orig) / std::pow(denom, 1.5);
        if (kappa > maxK)
            maxK = kappa;
    }

    return maxK;
}

// =============================================================================
// checkSagAndChord
// Computes sag_pixels and below_chord_ok on the top boundary.
// Returns true if both conditions pass.
//
// (e-1) sag check:
//   edge columns = leftmost 10% and rightmost 10%
//   center columns = 40%~60%
//   sag = avg_y_center - avg_y_edge   (positive = center lower than edges)
//   pass condition: sag >= min_sag_pixels
//
// (e-2) below-chord check:
//   chord: line connecting (col_first, y_first) to (col_last, y_last)
//   pass condition: all column y values >= chord_y(col) - chord_tolerance_px
//   i.e., no point is above the chord by more than tolerance
// =============================================================================
bool BedDetector::checkSagAndChord(BedCandidate& candidate, double min_sag_pixels,
                                   double chord_tolerance_px) {
    const int n = static_cast<int>(candidate.top_boundary_cols.size());
    if (n < 3) {
        candidate.sag_pixels = 0.0;
        candidate.below_chord_ok = false;
        return false;
    }

    // Sort by column index to ensure ordered traversal
    // (extractTopBoundary fills in order, but verify)
    // We assume they are already sorted; build index mapping if not needed.

    const auto& cols = candidate.top_boundary_cols;
    const auto& rows = candidate.top_boundary_rows;

    int col_min = cols.front();
    int col_max = cols.back();
    int col_span = col_max - col_min;
    if (col_span < 1) {
        candidate.sag_pixels = 0.0;
        candidate.below_chord_ok = false;
        return false;
    }

    // ----- (e-1) sag check -----
    // Edge zone: leftmost 10% and rightmost 10% of column range
    // Center zone: 40%~60%
    const double edge_frac = 0.10;
    const double center_low = 0.40;
    const double center_high = 0.60;

    double y_edge_sum = 0.0;
    int y_edge_cnt = 0;
    double y_center_sum = 0.0;
    int y_center_cnt = 0;

    for (int i = 0; i < n; ++i) {
        double frac = static_cast<double>(cols[i] - col_min) / col_span;
        if (frac <= edge_frac || frac >= 1.0 - edge_frac) {
            y_edge_sum += rows[i];
            ++y_edge_cnt;
        }
        if (frac >= center_low && frac <= center_high) {
            y_center_sum += rows[i];
            ++y_center_cnt;
        }
    }

    if (y_edge_cnt < 1 || y_center_cnt < 1) {
        candidate.sag_pixels = 0.0;
        candidate.below_chord_ok = false;
        return false;
    }

    double y_edge_mean = y_edge_sum / y_edge_cnt;
    double y_center_mean = y_center_sum / y_center_cnt;
    candidate.sag_pixels = y_center_mean - y_edge_mean;

    bool sag_ok = (candidate.sag_pixels >= min_sag_pixels);

    // ----- (e-2) below-chord check -----
    // Chord from first point to last point in the boundary
    double col_first = static_cast<double>(cols.front());
    double col_last = static_cast<double>(cols.back());
    double row_first = static_cast<double>(rows.front());
    double row_last = static_cast<double>(rows.back());

    double chord_slope =
        (col_last > col_first) ? (row_last - row_first) / (col_last - col_first) : 0.0;

    bool chord_ok = true;
    for (int i = 0; i < n; ++i) {
        double chord_y = row_first + chord_slope * (cols[i] - col_first);
        // "below chord" means row[i] >= chord_y  (row increases downward)
        // Allow points up to chord_tolerance_px ABOVE chord (smaller row value)
        if (static_cast<double>(rows[i]) < chord_y - chord_tolerance_px) {
            chord_ok = false;
            break;
        }
    }
    candidate.below_chord_ok = chord_ok;

    return sag_ok && chord_ok;
}

// =============================================================================
// extractTopBoundary
// For each column across the full image width, find the minimum row (top pixel)
// that is non-zero (foreground) within the vertical scan band derived from the
// component centroid.
//
// Column scan spans the full image width (0 … width-1), not a radius-derived
// bound from VD area. VD area is the sum of Voronoi disk generator cell areas
// — NOT the pixel count of the physical region. For the CT scanner bed, VD
// area ≈ 1700 (radius ≈ 24 px) while the actual bed spans 70~80% of the
// 512-px image width (≈ 360~410 px); a radius-derived column bound would
// underestimate width_px by roughly 5x.
//
// Row scan retains the centroid-based band: [cy_px - scan_margin, cy_px + scan_margin]
// where scan_margin = sqrt(VD_area / pi) * 1.5 (sufficient for vertical extent).
// The top-boundary width reflects the true physical extent of the bed layer.
//
// Scan range summary:
//   col: 0 … width-1   (full image width — no longer area-constrained)
//   row: max(0, cy_px - scan_margin) … min(height-1, cy_px + scan_margin)
// =============================================================================
void BedDetector::extractTopBoundary(const uint16_t* image, int width, int height,
                                     const VDComponentInfo& vd, BedCandidate& candidate) {
    candidate.top_boundary_rows.clear();
    candidate.top_boundary_cols.clear();
    candidate.bottom_boundary_rows.clear();  // reset bottom boundary

    if (!image || width <= 0 || height <= 0)
        return;

    // Centroid in pixel space.
    // V-IP coordinate convention (BetaUniverse2D::constructForImageProcessing):
    //   x = col - width/2 + 0.5    (col increases right, x increases right)
    //   y = row - height/2 + 0.5   (row increases down,  y increases down)
    //   pixel_row ≈ voro_y + height/2,  pixel_col ≈ voro_x + width/2.
    const double cy_px = static_cast<double>(height) / 2.0 + vd.centroid_y;

    // Row-only scan band: centroid ± radius * 1.5
    // This is sufficient to capture the vertical extent of the bed layer.
    const double radius = std::sqrt(vd.area / M_PI);
    const double scan_margin = radius * 1.5;

    const int row_start = std::max(0, static_cast<int>(std::floor(cy_px - scan_margin)));
    const int row_end = std::min(height - 1, static_cast<int>(std::ceil(cy_px + scan_margin)));

    // Column scan: full image width.
    // The VD component area does not represent the pixel-space extent of the bed,
    // so we cannot use it to limit the column range.
    const int col_start = 0;
    const int col_end = width - 1;

    for (int c = col_start; c <= col_end; ++c) {
        int top_row = -1;
        int bottom_row = -1;

        // Forward scan: find topmost (minimum row) foreground pixel
        for (int r = row_start; r <= row_end; ++r) {
            if (image[r * width + c] > 0) {
                top_row = r;
                break;
            }
        }

        if (top_row < 0)
            continue;  // no foreground in this column

        // Reverse scan from row_end downward to find bottommost foreground pixel
        for (int r = row_end; r >= top_row; --r) {
            if (image[r * width + c] > 0) {
                bottom_row = r;
                break;
            }
        }
        if (bottom_row < 0)
            bottom_row = top_row;  // single pixel: top == bottom

        candidate.top_boundary_rows.push_back(top_row);
        candidate.top_boundary_cols.push_back(c);
        candidate.bottom_boundary_rows.push_back(bottom_row);
    }

    // width_px: span of columns that have a valid top-boundary pixel
    if (!candidate.top_boundary_cols.empty()) {
        int cmin = *std::min_element(candidate.top_boundary_cols.begin(),
                                     candidate.top_boundary_cols.end());
        int cmax = *std::max_element(candidate.top_boundary_cols.begin(),
                                     candidate.top_boundary_cols.end());
        candidate.width_px = static_cast<double>(cmax - cmin + 1);
    } else {
        candidate.width_px = 0.0;
    }
}

// =============================================================================
// isBedCandidate
// Six conditions (AND):
// (a) position: centroid_y (pixel row) > body0_cy_px + theta_pos
// (b) circularity <= theta_circularity
// (c) width_px >= theta_size * image_width
// (d) max_curvature <= theta_curv
// (e) general convexity: sag check + below-chord check
// (f) avg thickness >= min_thickness_px  (cloth exclusion: cloth is thin)
// =============================================================================
bool BedDetector::isBedCandidate(const VDComponentInfo& vd, double body0_cy, int image_width,
                                 int image_height, const uint16_t* image, const Params& params,
                                 BedCandidate& candidate_out, DebugRecord& debug_out) {
    // Populate basic fields
    candidate_out.component_id = vd.id;
    candidate_out.centroid_x = vd.centroid_x;
    candidate_out.centroid_y = vd.centroid_y;
    candidate_out.area = vd.area;
    candidate_out.circularity = vd.circularity;

    // Populate debug basic fields (always recorded regardless of fail stage)
    debug_out.component_id = vd.id;
    debug_out.area = vd.area;
    debug_out.circularity = vd.circularity;
    debug_out.centroid_y = vd.centroid_y;
    debug_out.centroid_x = vd.centroid_x;
    debug_out.score = vd.score;

    // Convert Voronoi centroid_y to pixel row for position check.
    //
    // V-IP stores y = row - height/2 + offset (y-down, same as image row direction).
    // CoordinateTransform.h: pixel_row = vy + imageHeight/2 - COORDINATE_OFFSET.
    // For centroid comparison we use the integer-free form:
    //   pixel_row ≈ voro_y + height/2
    const double cy_px = static_cast<double>(image_height) / 2.0 + vd.centroid_y;
    const double body0_cy_px = static_cast<double>(image_height) / 2.0 + body0_cy;

    // (a) position check: bed must be below (larger row) than body0
    if (cy_px <= body0_cy_px + params.theta_pos) {
        debug_out.fail_reason = "pos";
        return false;
    }

    // (b) circularity check: bed has low circularity
    if (vd.circularity > params.theta_circularity) {
        debug_out.fail_reason = "circ";
        return false;
    }

    // Extract top boundary for remaining checks
    extractTopBoundary(image, image_width, image_height, vd, candidate_out);
    debug_out.width_px = candidate_out.width_px;

    // (c) width check: bed spans at least theta_size fraction of image width
    if (candidate_out.width_px < params.theta_size * image_width) {
        debug_out.fail_reason = "width";
        return false;
    }

    // Early exit if too few boundary points for poly fit
    const int n_pts = static_cast<int>(candidate_out.top_boundary_cols.size());
    if (n_pts < params.min_boundary_pts) {
        debug_out.fail_reason = "width";  // treat as width/insufficient boundary
        return false;
    }

    // Poly fit for (d) curvature check
    {
        std::vector<double> x_d(candidate_out.top_boundary_cols.begin(),
                                candidate_out.top_boundary_cols.end());
        std::vector<double> y_d(candidate_out.top_boundary_rows.begin(),
                                candidate_out.top_boundary_rows.end());

        candidate_out.poly_coeffs = fitPolynomial(x_d, y_d, params.poly_degree);
        if (candidate_out.poly_coeffs.empty()) {
            debug_out.fail_reason = "curv";  // poly fit failure treated as curvature fail
            return false;
        }

        // Pass the original pixel column range so maxCurvature converts to
        // original-domain curvature [1/px] for comparison with theta_curv.
        const double x_col_min = static_cast<double>(candidate_out.top_boundary_cols.front());
        const double x_col_max = static_cast<double>(candidate_out.top_boundary_cols.back());
        candidate_out.max_curvature =
            maxCurvature(candidate_out.poly_coeffs, x_col_min, x_col_max, 50);
        debug_out.max_curvature = candidate_out.max_curvature;
    }

    // (d) curvature check
    if (candidate_out.max_curvature > params.theta_curv) {
        debug_out.fail_reason = "curv";
        return false;
    }

    // (e) general convexity check
    bool convex_ok =
        checkSagAndChord(candidate_out, params.min_sag_pixels, params.chord_tolerance_px);
    debug_out.sag_pixels = candidate_out.sag_pixels;
    debug_out.below_chord_ok = candidate_out.below_chord_ok;
    if (!convex_ok) {
        debug_out.fail_reason = "convex";
        return false;
    }

    // (f) thickness check: cloth false-positive exclusion.
    // Cloth lies directly beneath the body (thin, 5~15 px vertical extent).
    // Real CT bed is a thick flat plate (30+ px vertical extent, MM20 observed).
    // Metric: average of (bottom_row - top_row) across all boundary columns.
    // top_boundary_rows and bottom_boundary_rows are parallel vectors filled by
    // extractTopBoundary(). If bottom_boundary_rows is empty or mismatched,
    // skip this condition conservatively (do not reject).
    {
        const int n_bnd = static_cast<int>(candidate_out.top_boundary_rows.size());
        const int n_bot = static_cast<int>(candidate_out.bottom_boundary_rows.size());
        if (n_bnd > 0 && n_bot == n_bnd) {
            double thickness_sum = 0.0;
            for (int i = 0; i < n_bnd; ++i) {
                thickness_sum += static_cast<double>(candidate_out.bottom_boundary_rows[i] -
                                                     candidate_out.top_boundary_rows[i]);
            }
            const double avg_thickness = thickness_sum / static_cast<double>(n_bnd);
            debug_out.thickness_px = avg_thickness;

            if (avg_thickness < params.min_thickness_px) {
                debug_out.fail_reason = "thick_low";
                return false;
            }
        }
        // If boundary data is absent, conservatively skip (f) and proceed.
    }

    debug_out.fail_reason = "passed";
    return true;
}

// =============================================================================
// detect
// Main detection entry point.
// =============================================================================
BedDetector::Result BedDetector::detect(const std::vector<VDComponentInfo>& vd_components,
                                        const uint16_t* image, int width, int height,
                                        double body0_cy, const Params& params) {
    Result result;
    if (vd_components.empty() || !image)
        return result;

    // Identify current body0 id (score-maximum = index 0)
    const int current_body0_id = vd_components[0].id;

    bool body0_is_bed = false;

    // body0 is evaluated through isBedCandidate() exactly like every other
    // component. In the common case body0's own centroid_y is only a few
    // pixels away from body0_cy_px (both derived from the same anatomical
    // region), which is far below theta_pos (50 px) — so condition (a)
    // rejects body0 immediately and behaviour is unchanged from before.
    // Only when body0 genuinely satisfies all bed conditions (e.g. the bed
    // itself was mis-selected as the highest-score component) does it get
    // added to bed_candidates and body0_is_bed is set, enabling the
    // replacement logic below.
    for (const auto& comp : vd_components) {
        BedCandidate candidate;
        DebugRecord drec;

        if (isBedCandidate(comp, body0_cy, width, height, image, params, candidate, drec)) {
            result.bed_candidates.push_back(std::move(candidate));
            if (comp.id == current_body0_id) {
                body0_is_bed = true;
            }
        }
        result.debug_records.push_back(std::move(drec));
    }

    // Apply layer count constraint
    const int n_found = static_cast<int>(result.bed_candidates.size());
    if (n_found < params.n_layers_min || n_found > params.n_layers_max) {
        // Outside expected range — treat as not detected
        result.bed_candidates.clear();
        result.detected = false;
        result.n_layers = 0;
        result.new_body0_component_id = -1;

        // Build envelope with valid = false
        result.envelope.width = width;
        result.envelope.y_at_col.assign(width, -1);
        result.envelope.bottom_y_at_col.assign(width, -1);
        result.envelope.valid = false;
        return result;
    }

    result.detected = (n_found >= params.n_layers_min);
    result.n_layers = n_found;

    // Build lower envelope from all bed candidates
    // top y: column-wise minimum top row across all bed candidates
    // bottom y: column-wise maximum bottom row across all bed candidates
    result.envelope.width = width;
    result.envelope.y_at_col.assign(width, -1);
    result.envelope.bottom_y_at_col.assign(width, -1);
    result.envelope.valid = result.detected;

    for (const auto& cand : result.bed_candidates) {
        const int n_pts = static_cast<int>(cand.top_boundary_cols.size());
        for (int i = 0; i < n_pts; ++i) {
            int c = cand.top_boundary_cols[i];
            int top_r = cand.top_boundary_rows[i];
            int bot_r = (i < static_cast<int>(cand.bottom_boundary_rows.size()))
                            ? cand.bottom_boundary_rows[i]
                            : top_r;  // fallback: no bottom data → use top

            if (c >= 0 && c < width) {
                // top y: take minimum (topmost row)
                if (result.envelope.y_at_col[c] < 0 || top_r < result.envelope.y_at_col[c]) {
                    result.envelope.y_at_col[c] = top_r;
                }
                // bottom y: take maximum (bottommost row)
                if (result.envelope.bottom_y_at_col[c] < 0 ||
                    bot_r > result.envelope.bottom_y_at_col[c]) {
                    result.envelope.bottom_y_at_col[c] = bot_r;
                }
            }
        }
    }

    // Top_y row clamp: any column whose envelope.y_at_col[col] is above
    // (smaller row than) image_height * top_clamp_ratio gets forced down to
    // that clamp row. Prevents a cloth+bed merged component's top boundary
    // from reaching into the body region (upper part of the image).
    // bottom_y_at_col is not clamped — the bed bottom is always near the
    // image bottom, so it already lies below the clamp row.
    if (result.detected && params.top_clamp_ratio > 0.0) {
        const int top_clamp_row =
            static_cast<int>(static_cast<double>(height) * params.top_clamp_ratio);
        for (int col = 0; col < width; ++col) {
            if (result.envelope.y_at_col[col] >= 0 &&
                result.envelope.y_at_col[col] < top_clamp_row) {
                result.envelope.y_at_col[col] = top_clamp_row;
            }
        }
    }

    // Determine new body0 component id
    if (result.detected && body0_is_bed) {
        // Find best non-bed component (highest score)
        double best_score = -1.0;
        int best_id = -1;

        // Collect bed ids
        std::vector<int> bed_ids;
        for (const auto& cand : result.bed_candidates)
            bed_ids.push_back(cand.component_id);

        for (const auto& comp : vd_components) {
            bool is_bed = (std::find(bed_ids.begin(), bed_ids.end(), comp.id) != bed_ids.end());
            if (!is_bed && comp.score > best_score) {
                best_score = comp.score;
                best_id = comp.id;
            }
        }
        result.new_body0_component_id = best_id;
    } else {
        // body0 is not bed or nothing detected — no replacement
        result.new_body0_component_id = -1;
    }

    return result;
}

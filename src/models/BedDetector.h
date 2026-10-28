#pragma once

#include <string>
#include <vector>
#include <cstdint>

// =============================================================================
// BedDetector
//
// Engine-layer class. Detects CT bed components in a single slice.
// Determines new body0 component id (no image modification).
//
// Design constraints:
//   - Uses VD component info from ImageProcessingBC::enumerate_components().
//   - Top boundary extraction uses pixel scan (no VD equivalent).
//   - Polynomial fit uses std::vector-based OLS (no external libs).
//   - isBedCandidate() checks: position + circularity + width + curvature +
//     general convexity (sag check + below-chord check).
//   - Fully no-op when disabled.
//   - Result stores IDs only (no raw pointers) — dangling pointer safe.
//
// Coordinate convention: image row-major, row increases downward (y-axis down).
// Voronoi space: V-IP places pixel (col, row) at (x, y) where
//   x = col - width/2 + offset,   y = row - height/2 + offset.
// Both axes increase in the same direction as image coordinates (y-down, x-right).
// Conversion: pixel_row ≈ voro_y + height/2,  pixel_col ≈ voro_x + width/2.
// =============================================================================

class BedDetector {
public:
    // -------------------------------------------------------------------------
    // VDComponentInfo: engine-side struct populated from
    // ImageProcessingBC::ComponentInfo.
    // -------------------------------------------------------------------------
    struct VDComponentInfo {
        int    id;
        double centroid_x;
        double centroid_y;
        double area;
        double circularity;
        double score;          // area * circularity^2
    };

    // -------------------------------------------------------------------------
    // DebugRecord: per-component 6-condition inspection result.
    // Populated in detect() for every VD component regardless of pass/fail.
    // fail_reason values: "pos" / "circ" / "width" / "curv" / "convex" /
    //                     "thick_low" (v5: bed y-range too thin; v6: min_thickness_px=0 → 사실상 나타나지 않음) /
    //                     "passed"
    // Fields that are not computed (early-exit before that stage) are set to -1.0.
    // -------------------------------------------------------------------------
    struct DebugRecord {
        int         component_id  = -1;
        double      area          = -1.0;
        double      circularity   = -1.0;
        double      centroid_y    = -1.0;  // Voronoi coordinate
        double      centroid_x    = -1.0;  // Voronoi coordinate
        double      score         = -1.0;
        double      width_px      = -1.0;  // computed after (a)(b) pass; -1 if not reached
        double      max_curvature = -1.0;  // computed after (c) pass; -1 if not reached
        double      sag_pixels    = -1.0;  // computed in (e); -1 if not reached
        double      thickness_px  = -1.0;  // v5: avg(bottom_y - top_y); computed after (e); -1 if not reached
        bool        below_chord_ok = false;
        std::string fail_reason;           // "pos"/"circ"/"width"/"curv"/"convex"/"thick_low"/"passed"
    };

    // -------------------------------------------------------------------------
    // BedCandidate: VD component + pixel-space top boundary.
    // Stores IDs and scalar values only. No raw pointers.
    // -------------------------------------------------------------------------
    struct BedCandidate {
        int    component_id;
        double centroid_x;
        double centroid_y;
        double area;
        double circularity;
        std::vector<int> top_boundary_rows;    // min row per col (pixel-space)
        std::vector<int> top_boundary_cols;    // col indices with valid pixels
        std::vector<int> bottom_boundary_rows; // v4: max row per col (pixel-space); parallel to top_boundary_cols
        double width_px = 0.0;                 // bounding box width
        std::vector<double> poly_coeffs;       // polynomial coefficients [a0, a1, ..., ad]
        double max_curvature = 0.0;            // max absolute curvature of fitted poly
        double sag_pixels    = 0.0;            // center_avg_y - edge_avg_y (>0 = downward sag)
        bool   below_chord_ok = false;         // all top_boundary pts below chord line
    };

    // -------------------------------------------------------------------------
    // LowerEnvelope: for each column x, top and bottom boundary y across all
    // detected bed candidates. (auxiliary info — not used for image modification)
    //
    // v3: top y only (envelope-below-all substitution).
    // v4: added bottom_y_at_col — enables column-wise exact bed range substitution.
    //     Python dump script uses [top_y, bottom_y] range per column instead of
    //     blanket substitution from top_y to image bottom.
    // -------------------------------------------------------------------------
    struct LowerEnvelope {
        int width = 0;
        std::vector<int> y_at_col;           // size = width; -1 = no bed at this col (top y, column-wise minimum)
        std::vector<int> bottom_y_at_col;    // v4: size = width; -1 = no bed at this col (bottom y, column-wise maximum)
        bool valid = false;
    };

    // -------------------------------------------------------------------------
    // Params
    // -------------------------------------------------------------------------
    struct Params {
        double theta_curv         = 0.003;  // max curvature [1/px] (radius ~333px)
        double min_sag_pixels     = 1.5;    // sag check minimum (downward sag >= N px)
        // min_sag_pixels rationale: CT bed top boundary observed sag = 1.89~3.30 px
        // (5 slices, MM20). bed is a flat plate, not arched. 5.0 was too strict.
        // False-positive defense for spine/vertebrae is already provided by
        // (b) circularity <= 0.3 (spine is near-circular, circ > 0.3 => rejected).
        double chord_tolerance_px = 1.0;    // below-chord check allowance [px]
        double theta_pos          = 50.0;   // v5: 0.0 → 50.0. bed centroid_y (pixel row) >
                                            // body0_cy_px + theta_pos. Cloth false-positive
                                            // exclusion: cloth lies 5~30 px below body0,
                                            // real bed lies 96+ px below body0 (MM20 obs).
        double theta_size         = 0.4;    // min width ratio (fraction of image width)
        double theta_circularity  = 0.3;    // max circularity (bed has low circularity)
        double min_thickness_px   = 0.0;    // v6: 15.0 → 0.0 (사실상 폐기).
                                            // BFS row scan 측정 한계로 진짜 bed (실제 30+ px)도
                                            // 5~6 px로 측정됨. 임계값 무의미 → default=0으로 무력화.
                                            // 조건 자체는 유지 (향후 측정 정확화 후 재도입 가능).
        double top_clamp_ratio    = 0.4;    // v6 new: envelope top_y row clamp ratio.
                                            // envelope.y_at_col[col] < image_height * top_clamp_ratio
                                            // 인 컬럼은 y_at_col을 clamp_row로 강제.
                                            // 0.4 = 이미지 상단 40% 이상으로 bed top이 침범 불가.
                                            // 천(cloth) + bed 합쳐진 component의 envelope이
                                            // body 영역(상단)까지 올라오는 false mask 방지.
        int    n_layers_max       = 3;      // max bed layers
        int    n_layers_min       = 1;      // min bed layers required
        int    poly_degree        = 2;      // polynomial degree for curvature estimation
        int    min_boundary_pts   = 10;     // minimum top-boundary points for poly fit
    };

    // -------------------------------------------------------------------------
    // Result
    // -------------------------------------------------------------------------
    struct Result {
        std::vector<BedCandidate> bed_candidates;
        LowerEnvelope             envelope;
        bool                      detected = false;
        int                       n_layers = 0;
        int                       new_body0_component_id = -1;
        // -1: no body0 replacement needed
        // >=0: id of component to use as new body0

        // Debug: one record per VD component, populated during detect().
        // Not stored in BedCache (debug_records is always empty after cache hit).
        std::vector<DebugRecord>  debug_records;
    };

    // -------------------------------------------------------------------------
    // Primary interface
    // -------------------------------------------------------------------------

    // Detect bed components and determine new body0 id.
    //
    // vd_components : from ImageProcessingBC::enumerate_components() (score desc)
    //                 vd_components[0] = current body0 (highest score)
    // image         : read-only uint16 pixel array (height x width, row-major)
    // width/height  : image dimensions
    // body0_cy      : centroid_y of body0 (from shell[0] after extractOuterShell)
    // params        : detection parameters
    //
    // Algorithm:
    //   1. Apply isBedCandidate() to every component
    //   2. Collect bed_candidates
    //   3. If current body0 (vd_components[0]) is in bed_candidates:
    //      find non-bed component with max score => new_body0_component_id
    //   4. If no bed candidates: detected=false, new_body0_component_id=-1
    Result detect(const std::vector<VDComponentInfo>& vd_components,
                  const uint16_t* image, int width, int height,
                  double body0_cy, const Params& params);

    // -------------------------------------------------------------------------
    // Helpers (public for unit-testability)
    // -------------------------------------------------------------------------

    // OLS polynomial fit of degree `degree`. Returns coefficients [a0, a1, ..., ad]
    // where y = a0 + a1*x + a2*x^2 + ... Evaluated at normalised x.
    // Returns empty vector if fit fails (singular matrix, insufficient points).
    static std::vector<double> fitPolynomial(const std::vector<double>& x,
                                             const std::vector<double>& y,
                                             int degree);

    // Max absolute curvature of polynomial over x range [x0, x1].
    // Samples n_samples points and returns max |kappa| = |y''| / (1 + y'^2)^1.5
    static double maxCurvature(const std::vector<double>& coeffs,
                               double x0, double x1, int n_samples = 50);

    // Check sag and below-chord conditions on top-boundary.
    // Fills candidate.sag_pixels and candidate.below_chord_ok.
    // Returns true if both conditions pass.
    static bool checkSagAndChord(BedCandidate& candidate,
                                 double min_sag_pixels,
                                 double chord_tolerance_px);

private:
    // Extract top and bottom boundary (min/max row per column) for a VD component region.
    // Fills candidate.top_boundary_rows, top_boundary_cols, bottom_boundary_rows.
    // Scan range: rows from max(0, centroid_row - scan_margin) to min(height-1, centroid_row + scan_margin).
    // scan_margin = sqrt(area / pi) * 1.5 (approximate radius from VD area).
    void extractTopBoundary(const uint16_t* image, int width, int height,
                            const VDComponentInfo& vd, BedCandidate& candidate);

    // Check if a VD component qualifies as bed.
    // Six conditions (AND):
    // (a) centroid_y > body0_cy + theta_pos   (position: below body0)
    // (b) circularity <= theta_circularity    (shape: low circularity)
    // (c) width_px >= theta_size * image_width (width: wide component)
    // (d) max_curvature <= theta_curv          (curvature: nearly flat arc)
    // (e) general convexity: sag check + below-chord check
    // (f) avg thickness >= min_thickness_px    (v5: cloth exclusion — cloth is thin)
    // Fills candidate_out with computed metrics.
    // Fills debug_out with per-condition inspection result (fail_reason, metrics).
    // Returns true if all conditions pass.
    bool isBedCandidate(const VDComponentInfo& vd, double body0_cy,
                        int image_width, int image_height,
                        const uint16_t* image,
                        const Params& params,
                        BedCandidate& candidate_out,
                        DebugRecord& debug_out);
};

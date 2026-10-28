// =============================================================================
// exp_face_rho — Experiment E-face: per-face ρ_f analysis without VD/QT
//
// Enumerates all triangular faces of the 511×511 pixel grid directly (no
// Voronoi construction), computes the inscribed-circle radius ρ_f for each
// face using the quadratic-formula from BetaUniverse2D, then collects
// statistics for Hypotheses H1 and H2.
//
// Usage:
//   exp_face_rho <dicom_dir> <output_prefix> [beta=0.70710678] [n_sample_maps=1]
//
// Outputs (all in current working directory unless prefix contains a path):
//   stdout           — PARAMS / H1 / H2 / PIXEL_R summary lines
//   {prefix}_rho_f_hist.csv          — 200-bin histogram [0,1)
//   {prefix}_exceeding_faces.csv     — faces with ρ_f > √2/2
//   {prefix}_exceed_map_{idx}.bin    — 511×511×2 uint8 exceed maps
//
// Architecture: SKINPEELER Engine layer — no Qt dependency.
// PyMGOS API is NOT called; formulas are re-implemented as static helpers.
// =============================================================================

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <filesystem>
#include <cstdint>
#include <cassert>

// DCMTK (for DICOM metadata tags only — same pattern as CTImage)
#include <dcmtk/dcmdata/dctk.h>

// SKINPEELER models
#include "models/CTImage.h"
#include "models/CTShell.h"
#include "config/Settings.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

namespace fs = std::filesystem;
using namespace std;

// =============================================================================
// File discovery (identical to exp_beta_sweep)
// =============================================================================
static int extractFileNumber(const string& path)
{
    string name = fs::path(path).filename().string();
    size_t dot = name.rfind('.');
    if (dot == string::npos) return 0;
    size_t end = dot;
    size_t start = end;
    while (start > 0 && isdigit(name[start - 1])) --start;
    if (start == end) return 0;
    return stoi(name.substr(start, end - start));
}

static vector<string> findDicomFiles(const string& directory)
{
    vector<string> paths;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        string ext = entry.path().extension().string();
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".dcm")
            paths.push_back(entry.path().string());
    }
    stable_sort(paths.begin(), paths.end(),
        [](const string& a, const string& b) {
            return extractFileNumber(a) < extractFileNumber(b);
        });
    return paths;
}

// =============================================================================
// Cross-slice normalization (identical to exp_beta_sweep)
// =============================================================================
static void normalizeIntensities(vector<unique_ptr<CTImage>>& images)
{
    double globalMax = 0.0;
    for (const auto& img : images)
        globalMax = max(globalMax, img->imageMax());
    if (globalMax <= 0.0) globalMax = 1.0;
    for (auto& img : images)
        img->normalizeToMax(globalMax);
    cerr << "[exp_face_rho] globalMax = " << globalMax << "\n";
}

// =============================================================================
// Re-implementation of BetaUniverse2D formulas (PyMGOS API not called)
// =============================================================================

// calculateRadiusOfPixel  (BetaUniverse2D.cpp:1779-1782)
// DARKER_PIXEL_BIGGER_DISK = true  →  r = scaled / 255.0 / 2.0
static inline double radiusOfPixel(int scaled8bit)
{
    // DARKER_PIXEL_BIGGER_DISK=true: darker=bigger, so brighter→smaller radius
    // Formula: r = graylevel / 2.0  where graylevel = intensity / 255.0
    // With DARKER_PIXEL_BIGGER_DISK=true (see Settings.h):
    //   r = graylevelIntensity / 2.0
    double g = static_cast<double>(scaled8bit) / 255.0;
    return DARKER_PIXEL_BIGGER_DISK ? g / 2.0 : (1.0 - g) / 2.0;
}

// calculateRadiusOfTangentCircle_quadraticFormula  (BetaUniverse2D.cpp:1840-1858)
// Returns ρ_f (the inscribed-circle radius for the face).
// Robustness guards added per spec.
static double compute_rho_f(double r_base, double r_prev_ccw, double r_next_ccw)
{
    double r_n = r_next_ccw - r_base;
    double r_p = r_prev_ccw - r_base;

    double r_n2 = r_n * r_n;
    double r_p2 = r_p * r_p;
    double r_n3 = r_n2 * r_n;
    double r_p3 = r_p2 * r_p;
    double r_n4 = r_n2 * r_n2;
    double r_p4 = r_p2 * r_p2;

    double a = 4.0 * (1.0 - r_n2 - r_p2);
    double b = 2.0 * (r_n + r_p - r_n3 - r_p3);
    double c = 2.0 * (r_n2 + r_p2) - r_n4 - r_p4 - 2.0;

    if (std::abs(a) < 1e-12) return 0.0;

    double disc = b * b - a * c;
    if (disc < 0.0) return 0.0;

    double enlarged_radius = (std::sqrt(disc) - b) / a;
    double rho_f = enlarged_radius - r_base;

    return (rho_f < 0.0) ? 0.0 : rho_f;
}

// diagonal_is_positive  (BetaUniverse2D.cpp:1785-1815)
// Arguments: (leftBottom, rightBottom, leftTop, rightTop) radii.
// Returns true if the diagonal tilts "right-up" (positive slope).
static bool diagonal_is_positive(double r_lb, double r_rb, double r_lt, double r_rt)
{
    double baseRadius = r_lb;
    double r_n = r_rb - baseRadius;   // rightBottom relative
    double r_p = r_lt - baseRadius;   // leftTop relative

    double r_n2 = r_n * r_n;
    double r_p2 = r_p * r_p;
    double r_n3 = r_n2 * r_n;
    double r_p3 = r_p2 * r_p;
    double r_n4 = r_n2 * r_n2;
    double r_p4 = r_p2 * r_p2;

    double a = 4.0 * (1.0 - r_n2 - r_p2);
    double b = 2.0 * (r_n + r_p - r_n3 - r_p3);
    double c = 2.0 * (r_n2 + r_p2) - r_n4 - r_p4 - 2.0;

    double disc = b * b - a * c;
    if (disc < 0.0 || std::abs(a) < 1e-12) return false;

    double enlarged_radius = (std::sqrt(disc) - b) / a;
    double r_tangent = enlarged_radius - baseRadius;

    double x_tc = -(r_n2 + 2.0 * r_n * enlarged_radius - 1.0) / 2.0;
    double y_tc = -(r_p2 + 2.0 * r_p * enlarged_radius - 1.0) / 2.0;

    // Distance from rightTop corner (1,1) in normalized cell coords
    double dx = 1.0 - x_tc;
    double dy = 1.0 - y_tc;
    double dist_sq = dx * dx + dy * dy;
    double sum_r = r_tangent + r_rt;
    return (dist_sq < sum_r * sum_r);
}

// =============================================================================
// Per-face data structure
// =============================================================================
struct FaceInfo {
    int    slice_idx;
    int    face_x;    // cell col index i  (0..510)
    int    face_y;    // cell row index j  (0..510)
    int    face_half; // 0=lower, 1=upper
    double r_b;       // baseRadius (right-angle vertex)
    double r_n;       // nextRadius_CCW
    double r_p;       // prevRadius_CCW
    double rho_f;
};

// is_biggest_result codes
enum class BiggestResult { YES, NO, SAME };

static BiggestResult isBiggest(double r_b, double r_n, double r_p)
{
    double mx = max(r_n, r_p);
    if (r_b > mx) return BiggestResult::YES;
    if (r_b < mx) return BiggestResult::NO;
    return BiggestResult::SAME;
}

// =============================================================================
// Percentile computation on a sorted vector
// =============================================================================
static double percentile(const vector<double>& sorted_vals, double p)
{
    if (sorted_vals.empty()) return 0.0;
    double idx = p / 100.0 * static_cast<double>(sorted_vals.size() - 1);
    size_t lo = static_cast<size_t>(idx);
    size_t hi = lo + 1;
    if (hi >= sorted_vals.size()) return sorted_vals.back();
    double frac = idx - static_cast<double>(lo);
    return sorted_vals[lo] * (1.0 - frac) + sorted_vals[hi] * frac;
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[])
{
    if (argc < 3) {
        cerr << "Usage: " << argv[0]
             << " <dicom_dir> <output_prefix> [beta=0.70710678] [n_sample_maps=1]\n";
        return 1;
    }

    const string dicom_dir      = argv[1];
    const string output_prefix  = argv[2];
    const double beta           = (argc > 3) ? stod(argv[3]) : M_SQRT2 / 2.0;
    const int    n_sample_maps  = (argc > 4) ? stoi(argv[4]) : 1;

    cerr << "[exp_face_rho] dicom_dir=" << dicom_dir
         << "  prefix=" << output_prefix
         << "  beta=" << beta
         << "  n_sample_maps=" << n_sample_maps << "\n";

    // -------------------------------------------------------------------------
    // Discover DICOM files
    // -------------------------------------------------------------------------
    vector<string> files;
    try {
        files = findDicomFiles(dicom_dir);
    } catch (const exception& e) {
        cerr << "[exp_face_rho] Directory error: " << e.what() << "\n";
        return 1;
    }
    if (files.empty()) {
        cerr << "[exp_face_rho] No .dcm files found.\n";
        return 1;
    }
    cerr << "[exp_face_rho] Found " << files.size() << " file(s).\n";

    // -------------------------------------------------------------------------
    // 1st pass: load all slices, extract metadata, compute globalMax
    // -------------------------------------------------------------------------
    cerr << "[exp_face_rho] Pass 1: loading slices...\n";
    vector<unique_ptr<CTImage>> images;
    images.reserve(files.size());
    for (const auto& path : files) {
        try {
            images.push_back(make_unique<CTImage>(
                CTImage::fromDicomFile(path, DEFAULT_BETA, DEFAULT_SHELL_THICKNESS)));
        } catch (const exception& e) {
            cerr << "  Skipping " << fs::path(path).filename().string()
                 << ": " << e.what() << "\n";
        }
    }
    if (images.empty()) {
        cerr << "[exp_face_rho] No slices loaded successfully.\n";
        return 1;
    }

    // Cross-slice normalization (updates m_image in each CTImage)
    normalizeIntensities(images);
    cerr << "[exp_face_rho] Loaded and normalized " << images.size() << " slice(s).\n";

    // -------------------------------------------------------------------------
    // Extract DICOM metadata from first successfully loaded file
    // (WC, WW, RescaleSlope, RescaleIntercept, BitsStored, minVal)
    // -------------------------------------------------------------------------
    double meta_wc = 0.0, meta_ww = 0.0;
    double meta_slope = 1.0, meta_intercept = 0.0;
    int    meta_bitsStored = 16;
    int    meta_minVal = 0;

    {
        // Load the DICOM file raw to read metadata tags
        auto ff = std::make_unique<DcmFileFormat>();
        if (ff->loadFile(files[0].c_str()).good()) {
            DcmDataset* ds = ff->getDataset();
            if (ds) {
                Float64 wc = 0.0, ww = 0.0;
                DcmElement* wcElem = nullptr;
                DcmElement* wwElem = nullptr;
                if (ds->findAndGetElement(DCM_WindowCenter, wcElem).good() && wcElem)
                    wcElem->getFloat64(wc, 0);
                if (ds->findAndGetElement(DCM_WindowWidth, wwElem).good() && wwElem)
                    wwElem->getFloat64(ww, 0);
                meta_wc = wc;
                meta_ww = ww;

                Float64 slope = 1.0, intercept = 0.0;
                ds->findAndGetFloat64(DCM_RescaleSlope,     slope,     0);
                ds->findAndGetFloat64(DCM_RescaleIntercept, intercept, 0);
                meta_slope     = slope;
                meta_intercept = intercept;

                Uint16 bs = 16;
                ds->findAndGetUint16(DCM_BitsStored, bs);
                meta_bitsStored = static_cast<int>(bs);
            }
        }
        // minVal: minimum of the first slice's processed image before normalisation
        // (extractImageFromDicom subtracts the min internally, so the raw min after
        //  extraction is always 0; we report the min of the processed image post-norm)
        const auto& img0 = *images[0];
        const auto& pix0 = img0.image();
        if (!pix0.empty())
            meta_minVal = static_cast<int>(*std::min_element(pix0.begin(), pix0.end()));
    }

    // -------------------------------------------------------------------------
    // Determine which slice indices to write exceed maps for
    // -------------------------------------------------------------------------
    const int nSlices = static_cast<int>(images.size());
    vector<int> map_slice_indices;
    {
        int nm = max(1, min(n_sample_maps, nSlices));
        if (nm == 1) {
            map_slice_indices.push_back(nSlices / 2);
        } else {
            for (int k = 0; k < nm; ++k)
                map_slice_indices.push_back(
                    static_cast<int>(round(static_cast<double>(k) * (nSlices - 1) / (nm - 1))));
        }
    }

    // -------------------------------------------------------------------------
    // Open output files
    // -------------------------------------------------------------------------
    ofstream csv_exc(output_prefix + "_exceeding_faces.csv");
    if (!csv_exc.is_open()) {
        cerr << "[exp_face_rho] Cannot open exceeding_faces CSV.\n";
        return 1;
    }
    csv_exc << fixed << setprecision(6);
    csv_exc << "slice_idx,face_x,face_y,face_half,r_b,r_n,r_p,rho_f\n";

    // ρ_f histogram accumulators (200 bins, [0.0, 1.0), step 0.005)
    constexpr int    N_BINS    = 200;
    constexpr double BIN_STEP  = 0.005;
    vector<long long> hist(N_BINS, 0LL);

    // H1 counters (global, accumulated over all slices)
    long long total_exceeding       = 0;
    long long cond_yes              = 0;   // exceeds AND r_b > max(r_n,r_p)
    long long cond_no               = 0;   // exceeds AND r_b < max(r_n,r_p)
    long long cond_same             = 0;   // exceeds AND r_b == max(r_n,r_p)
    long long cond_yes_not_exceeding = 0;  // NOT exceeds AND r_b > max(r_n,r_p)

    // H2 counters
    long long adj_count[4] = {0, 0, 0, 0};  // adj_0..adj_3 = n_interior_adj faces
    long long dual_exceed  = 0;
    double    rho_f_max    = 0.0;

    // PIXEL_R collection
    vector<double> all_pixel_r;  // collect all r values for percentiles

    const double sqrt2_half = M_SQRT2 / 2.0;

    // -------------------------------------------------------------------------
    // 2nd pass: per-slice face enumeration
    // -------------------------------------------------------------------------
    cerr << "[exp_face_rho] Pass 2: face enumeration...\n";

    for (int slice_idx = 0; slice_idx < nSlices; ++slice_idx) {
        if (slice_idx % 50 == 0)
            cerr << "  slice " << slice_idx << "/" << nSlices << "\n";

        const CTImage& img = *images[slice_idx];
        const int W = img.width();
        const int H = img.height();
        if (W < 2 || H < 2) continue;

        const auto& pixels = img.image();  // uint16, row-major

        // bitsStored for this slice
        Uint16 bsU16 = 16;
        {
            auto ff2 = std::make_unique<DcmFileFormat>();
            if (ff2->loadFile(files[slice_idx].c_str()).good()) {
                DcmDataset* ds2 = ff2->getDataset();
                if (ds2) ds2->findAndGetUint16(DCM_BitsStored, bsU16);
            }
        }
        const int bitsStored  = static_cast<int>(bsU16);
        const int maxInputVal = (1 << bitsStored) - 1;

        // Build 2D radius grid  r[col][row]  (column-major, same as library)
        // Scaling: scaled = round(pixel * 255.0 / maxInputVal)
        //          r = scaled / 255.0 / 2.0  (DARKER_PIXEL_BIGGER_DISK=true)
        // We store as double r[col][row]
        // Memory: W * H doubles — for a 512x512 slice that is ~2 MB, acceptable.
        vector<vector<double>> r(W, vector<double>(H));
        for (int row = 0; row < H; ++row) {
            for (int col = 0; col < W; ++col) {
                uint16_t pix = pixels[row * W + col];
                int scaled = static_cast<int>(
                    std::round(static_cast<double>(pix) * 255.0 / maxInputVal));
                scaled = max(0, min(255, scaled));
                r[col][row] = radiusOfPixel(scaled);
                all_pixel_r.push_back(r[col][row]);
            }
        }

        // Build ρ_f grid  rho[col][row][half]  for H2 adjacency lookup
        // half 0 = lower face, half 1 = upper face
        // Dimensions: (W-1) x (H-1) x 2
        const int GW = W - 1;
        const int GH = H - 1;
        vector<vector<array<double, 2>>> rho(GW, vector<array<double, 2>>(GH, {0.0, 0.0}));

        // ----- First sub-pass: compute diagonal direction + ρ_f for all cells -----
        for (int i = 0; i < GW; ++i) {
            for (int j = 0; j < GH; ++j) {
                double r_lb = r[i  ][j  ];  // leftBottom  (i,   j  )
                double r_rb = r[i+1][j  ];  // rightBottom (i+1, j  )
                double r_lt = r[i  ][j+1];  // leftTop     (i,   j+1)
                double r_rt = r[i+1][j+1];  // rightTop    (i+1, j+1)

                bool pos_diag = diagonal_is_positive(r_lb, r_rb, r_lt, r_rt);

                double rho_lower, rho_upper;
                if (pos_diag) {
                    // positive diagonal: lowerFace base=rightBottom, upperFace base=leftTop
                    // BetaUniverse2D line 976:
                    //   lowerFace: computeRho(r_rightBottom, r_leftBottom, r_rightTop)
                    //   upperFace: computeRho(r_leftTop,     r_rightTop,   r_leftBottom)
                    rho_lower = compute_rho_f(r_rb, r_lb, r_rt);
                    rho_upper = compute_rho_f(r_lt, r_rt, r_lb);
                } else {
                    // negative diagonal: lowerFace base=leftBottom, upperFace base=rightTop
                    // BetaUniverse2D line 980:
                    //   lowerFace: computeRho(r_leftBottom, r_leftTop,    r_rightBottom)
                    //   upperFace: computeRho(r_rightTop,   r_rightBottom, r_leftTop)
                    rho_lower = compute_rho_f(r_lb, r_lt, r_rb);
                    rho_upper = compute_rho_f(r_rt, r_rb, r_lt);
                }

                rho[i][j][0] = rho_lower;
                rho[i][j][1] = rho_upper;
            }
        }

        // ----- Second sub-pass: collect stats + H2 -----
        // Prepare exceed map if this slice should be saved
        bool save_map = (std::find(map_slice_indices.begin(),
                                   map_slice_indices.end(), slice_idx)
                         != map_slice_indices.end());
        // exceed_map[row][col][half]: 1 if exceeds, 0 otherwise
        vector<uint8_t> exceed_map;
        if (save_map)
            exceed_map.assign(GH * GW * 2, 0);

        for (int i = 0; i < GW; ++i) {
            for (int j = 0; j < GH; ++j) {
                double r_lb = r[i  ][j  ];
                double r_rb = r[i+1][j  ];
                double r_lt = r[i  ][j+1];
                double r_rt = r[i+1][j+1];

                bool pos_diag = diagonal_is_positive(r_lb, r_rb, r_lt, r_rb);
                // Note: recompute for correctness — avoids caching bool separately
                pos_diag = diagonal_is_positive(r_lb, r_rb, r_lt, r_rt);

                // ---- lower face (half=0) ----
                {
                    double rb, rn, rp;
                    if (pos_diag) {
                        rb = r_rb; rn = r_rt; rp = r_lb;
                    } else {
                        rb = r_lb; rn = r_rb; rp = r_lt;
                    }
                    double rho_f = rho[i][j][0];

                    // Histogram
                    int bin = static_cast<int>(rho_f / BIN_STEP);
                    if (bin >= 0 && bin < N_BINS) hist[bin]++;

                    // PIXEL_R: nothing extra — already collected above

                    bool exceeds = (rho_f > sqrt2_half);
                    BiggestResult bg = isBiggest(rb, rn, rp);

                    if (exceeds) {
                        ++total_exceeding;
                        if (bg == BiggestResult::YES)  ++cond_yes;
                        else if (bg == BiggestResult::NO)   ++cond_no;
                        else                                ++cond_same;

                        if (rho_f > rho_f_max) rho_f_max = rho_f;

                        // Write to exceeding_faces.csv
                        csv_exc << slice_idx << "," << i << "," << j << ",0,"
                                << rb << "," << rn << "," << rp << "," << rho_f << "\n";

                        // H2: adjacency analysis (only for exceeding faces)
                        // Paired face = same cell, half=1 (upper)
                        // 3 adjacent faces for lower face:
                        //   1) Paired (same cell, half=1)
                        //   2) Cell (i, j-1) lower or upper (depends on diagonal)
                        //   3) Cell (i-1, j) lower or upper (depends on diagonal)
                        //
                        // Conservative rule: border faces count as INTERIOR (rho<beta)
                        int n_interior_adj = 0;

                        // Adj 1: paired upper face of same cell
                        {
                            double rho_adj = rho[i][j][1];
                            bool adj_exceeds = (rho_adj > sqrt2_half);
                            if (adj_exceeds) dual_exceed++;  // paired cell both exceed
                            if (!adj_exceeds) n_interior_adj++;  // rho<beta → interior
                        }

                        // Adj 2: cell below (j-1), lower face (shares bottom edge)
                        //   For positive diagonal: shares edge (i,j)-(i+1,j) → lower face of (i,j-1)
                        //   For negative diagonal: same
                        if (j > 0) {
                            double rho_adj = rho[i][j-1][0];
                            if (!(rho_adj > sqrt2_half)) n_interior_adj++;
                        } else {
                            n_interior_adj++;  // border → treat as interior
                        }

                        // Adj 3: cell to the left (i-1, j), upper face
                        if (i > 0) {
                            double rho_adj = rho[i-1][j][1];
                            if (!(rho_adj > sqrt2_half)) n_interior_adj++;
                        } else {
                            n_interior_adj++;  // border → treat as interior
                        }

                        if (n_interior_adj >= 0 && n_interior_adj <= 3)
                            adj_count[n_interior_adj]++;
                    } else {
                        if (bg == BiggestResult::YES) ++cond_yes_not_exceeding;
                    }

                    if (save_map) {
                        // row-major: [row=j][col=i][half=0]
                        exceed_map[(j * GW + i) * 2 + 0] = exceeds ? 1u : 0u;
                    }
                }

                // ---- upper face (half=1) ----
                {
                    double rb, rn, rp;
                    if (pos_diag) {
                        rb = r_lt; rn = r_lb; rp = r_rt;
                    } else {
                        rb = r_rt; rn = r_lt; rp = r_rb;
                    }
                    double rho_f = rho[i][j][1];

                    // Histogram
                    int bin = static_cast<int>(rho_f / BIN_STEP);
                    if (bin >= 0 && bin < N_BINS) hist[bin]++;

                    bool exceeds = (rho_f > sqrt2_half);
                    BiggestResult bg = isBiggest(rb, rn, rp);

                    if (exceeds) {
                        ++total_exceeding;
                        if (bg == BiggestResult::YES)  ++cond_yes;
                        else if (bg == BiggestResult::NO)   ++cond_no;
                        else                                ++cond_same;

                        if (rho_f > rho_f_max) rho_f_max = rho_f;

                        csv_exc << slice_idx << "," << i << "," << j << ",1,"
                                << rb << "," << rn << "," << rp << "," << rho_f << "\n";

                        // H2: adjacency analysis for upper face
                        // Adjacent faces:
                        //   1) Paired lower face (same cell, half=0)
                        //   2) Cell (i, j+1) upper face (shares top edge)
                        //   3) Cell (i+1, j) upper face (shares right edge)
                        int n_interior_adj = 0;

                        // Adj 1: paired lower face
                        {
                            double rho_adj = rho[i][j][0];
                            bool adj_exceeds = (rho_adj > sqrt2_half);
                            if (adj_exceeds) dual_exceed++;
                            if (!adj_exceeds) n_interior_adj++;
                        }

                        // Adj 2: cell above (j+1), upper face
                        if (j + 1 < GH) {
                            double rho_adj = rho[i][j+1][1];
                            if (!(rho_adj > sqrt2_half)) n_interior_adj++;
                        } else {
                            n_interior_adj++;
                        }

                        // Adj 3: cell to the right (i+1, j), lower face
                        if (i + 1 < GW) {
                            double rho_adj = rho[i+1][j][0];
                            if (!(rho_adj > sqrt2_half)) n_interior_adj++;
                        } else {
                            n_interior_adj++;
                        }

                        if (n_interior_adj >= 0 && n_interior_adj <= 3)
                            adj_count[n_interior_adj]++;
                    } else {
                        if (bg == BiggestResult::YES) ++cond_yes_not_exceeding;
                    }

                    if (save_map) {
                        exceed_map[(j * GW + i) * 2 + 1] = exceeds ? 1u : 0u;
                    }
                }
            }  // j
        }  // i

        // Write exceed map binary
        if (save_map && !exceed_map.empty()) {
            string map_path = output_prefix + "_exceed_map_"
                              + to_string(slice_idx) + ".bin";
            ofstream binf(map_path, ios::binary);
            if (binf.is_open()) {
                binf.write(reinterpret_cast<const char*>(exceed_map.data()),
                           static_cast<streamsize>(exceed_map.size()));
                cerr << "[exp_face_rho] Wrote " << map_path << "\n";
            } else {
                cerr << "[exp_face_rho] Warning: cannot write " << map_path << "\n";
            }
        }
    }  // slice_idx

    csv_exc.close();
    cerr << "[exp_face_rho] Pass 2 complete.\n";

    // -------------------------------------------------------------------------
    // Write histogram CSV
    // -------------------------------------------------------------------------
    {
        ofstream csv_hist(output_prefix + "_rho_f_hist.csv");
        if (!csv_hist.is_open()) {
            cerr << "[exp_face_rho] Warning: cannot write histogram CSV.\n";
        } else {
            csv_hist << fixed << setprecision(6);
            csv_hist << "bin_center,count\n";
            for (int b = 0; b < N_BINS; ++b) {
                double center = (b + 0.5) * BIN_STEP;
                csv_hist << center << "," << hist[b] << "\n";
            }
        }
    }

    // -------------------------------------------------------------------------
    // Compute PIXEL_R percentiles
    // -------------------------------------------------------------------------
    sort(all_pixel_r.begin(), all_pixel_r.end());
    double pr_p5  = percentile(all_pixel_r,  5.0);
    double pr_p25 = percentile(all_pixel_r, 25.0);
    double pr_p50 = percentile(all_pixel_r, 50.0);
    double pr_p75 = percentile(all_pixel_r, 75.0);
    double pr_p95 = percentile(all_pixel_r, 95.0);
    double pr_max = all_pixel_r.empty() ? 0.0 : all_pixel_r.back();

    // dual_exceed was counted once per exceeding face (both lower and upper),
    // so each pair is counted twice. Divide by 2.
    dual_exceed /= 2;

    // -------------------------------------------------------------------------
    // stdout summary
    // -------------------------------------------------------------------------
    cout << fixed << setprecision(6);
    cout << "PARAMS: WC=" << setprecision(1) << meta_wc
         << " WW=" << meta_ww
         << " globalMax=" << images[0]->imageMax()  // after normalization = IMAGE_MAX_VALUE
         << " bitsStored=" << meta_bitsStored
         << " RescaleSlope=" << setprecision(6) << meta_slope
         << " RescaleIntercept=" << meta_intercept
         << " minVal=" << meta_minVal << "\n";

    cout << "H1: total_exceeding=" << total_exceeding
         << " condition_yes=" << cond_yes
         << " condition_no=" << cond_no
         << " condition_same=" << cond_same
         << " condition_yes_not_exceeding=" << cond_yes_not_exceeding << "\n";

    cout << "H2: adj_0=" << adj_count[0]
         << " adj_1=" << adj_count[1]
         << " adj_2=" << adj_count[2]
         << " adj_3=" << adj_count[3]
         << " dual_exceed=" << dual_exceed
         << " rho_f_max=" << setprecision(6) << rho_f_max << "\n";

    cout << "PIXEL_R:"
         << " p5=" << pr_p5
         << " p25=" << pr_p25
         << " p50=" << pr_p50
         << " p75=" << pr_p75
         << " p95=" << pr_p95
         << " max=" << pr_max << "\n";

    cerr << "[exp_face_rho] Done.\n";
    return 0;
}

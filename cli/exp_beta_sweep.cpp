// =============================================================================
// exp_beta_sweep — Beta sweep experiment for SKINPEELER paper (Experiment E-extended)
//
// Loads DICOM slices, builds Voronoi ONCE per slice, then sweeps beta values and
// measures biggest-component metrics at each step using the library's own
// computeArea() / computeCircularity() values (ComponentBSFace internals).
// This ensures consistency with the selection logic used in the main pipeline.
//
// In addition to component geometry, shell statistics (peel_index, n_shells,
// Mahalanobis distances, per-shell mu/sigma) are computed for each (slice × beta)
// using the same 1-Voronoi per slice. Before each shell peeling pass,
// reset_vertex_visited() clears the vertex visited flags so that the inner-shell
// traversal is not corrupted by the previous β iteration.
//
// Outputs two CSV files:
//
//   <output_csv>            — component geometry (one row per slice × beta)
//     columns: slice, beta, n_components, n_faces, area, perimeter, circularity,
//              score, n_inner_loops, n_outer_edges, outer_circularity,
//              peel_index, n_shells, mahal_d_at_cut
//
//   <output_csv>.shells.csv — per-shell detail (one row per slice × beta × shell)
//     columns: slice, beta, shell_index, mu, sigma, n_vertices, mahal_d
//              (shell_index 0 = outermost; mahal_d = -1 for shell 0 per convention)
//
// Usage:
//   exp_beta_sweep <dicom_dir> <output_csv> [beta_min=0.50] [beta_max=0.90] [beta_step=0.01]
//
// Notes:
//   * DICOM loading uses CTImage directly (same pipeline as the GUI) — LUT
//     (WindowCenter/Width) is applied by default.
//   * File discovery uses std::filesystem — no Qt dependency.
//   * Shell statistics reuse the same Voronoi object as the component sweep:
//     1 Voronoi build per slice, reset_vertex_visited() before each β's shell pass.
//   * Shell peeling logic mirrors CTImage::calculateInnerShellsAdaptive() and
//     CTImage::cutting3SigmaDistance() — uses the same V-library public API
//     (QuasiVertex::find_incident_quasi_edges, QuasiEdge::get_bounding_state,
//     REGULAR_STATE / INTERIOR_STATE).
// =============================================================================

#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <numeric>

// V library
#include "ImageProcessingBC.h"
#include "QuasiVertex.h"
#include "QuasiEdge.h"
#include "VoroConst.h"

// SKINPEELER models
#include "models/CTImage.h"
#include "models/CTShell.h"
#include "config/Settings.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fs = std::filesystem;
using namespace std;
using namespace V;

// -----------------------------------------------------------------------------
// File discovery: find and sort .dcm files by trailing number
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Cross-slice normalization (matches GUI DicomService::normalizeIntensities)
// -----------------------------------------------------------------------------
static void normalizeIntensities(vector<unique_ptr<CTImage>>& images)
{
    double globalMax = 0.0;
    for (const auto& img : images)
        globalMax = max(globalMax, img->imageMax());
    if (globalMax <= 0.0) globalMax = 1.0;
    for (auto& img : images)
        img->normalizeToMax(globalMax);
}

// =============================================================================
// ShellData — lightweight shell record used within the beta sweep
// =============================================================================
struct ShellData {
    int    n_vertices = 0;
    double mu         = 0.0;
    double sigma      = 0.0;
};

// =============================================================================
// compute_shell_stats_inline
//
// Mirrors CTImage::calculateInnerShellsAdaptive() and
// CTImage::cutting3SigmaDistance() using V-library public API only.
//
// Pre-conditions:
//   - bc.set_beta_value(beta), bc.classifyByComponents(),
//     bc.select_biggest_componentBSFace(), bc.reset_vertex_visited() have been
//     called in this order.
//
// Returns: vector of ShellData (index 0 = outermost shell).
// Also returns peel_index (0-based into shells vector) and
// shell_mahal (per-shell Mahalanobis distance, -1 for shell 0).
//
// On failure (empty outer loop, Voronoi issue) returns empty shells.
// =============================================================================
static void compute_shell_stats_inline(
    ImageProcessingBC&  bc,
    double              beta,
    vector<ShellData>&  shells_out,
    int&                peel_index_out,
    vector<double>&     mahal_out)
{
    shells_out.clear();
    peel_index_out = 0;
    mahal_out.clear();

    // -------------------------------------------------------------------------
    // Step 1+2: Extract all shells (shell[0] = outer loop, then inner-shell BFS)
    // via the engine's leak-free raw-pointer traversal. The previous
    // QuasiVertex/QuasiEdge wrapper traversal leaked one QT*2DCore object on
    // every get_start_quasi_vertex / get_end_quasi_vertex /
    // find_incident_quasi_edges call (those ctors copy-without-owning the
    // `new`'d pointer), causing per-slice RSS growth in the β-sweep.
    // ImageProcessingBC::extract_shell_radius_stats mirrors the same logic with
    // raw VertexBU2D*/EdgeBU2D* and zero per-vertex allocation.
    // -------------------------------------------------------------------------
    vector<ImageProcessingBC::ShellRadiusStat> raw;
    bc.extract_shell_radius_stats(beta, ADAPTIVE_SHELL_MAX, raw);
    if (raw.empty()) return;

    shells_out.reserve(raw.size());
    for (const auto& rs : raw) {
        ShellData sh;
        sh.n_vertices = rs.n_vertices;
        if (rs.n_vertices > 0) {
            double n     = static_cast<double>(rs.n_vertices);
            double meanR = rs.sum_r / n;
            double varR  = rs.sum_r2 / n - meanR * meanR;
            if (varR < 0.0) varR = 0.0;
            sh.mu    = meanR * 2.0;
            sh.sigma = std::sqrt(varR) * 2.0;
        }
        shells_out.push_back(sh);
    }

    // -------------------------------------------------------------------------
    // Step 3: Mahalanobis distance + peel_index (mirrors cutting3SigmaDistance).
    // shells[0] is excluded from statistics (EXCLUDE_OUTERMOST_SHELL = true).
    // -------------------------------------------------------------------------
    const int nShells = static_cast<int>(shells_out.size());
    mahal_out.assign(nShells, -1.0);

    if (nShells < 2) {
        peel_index_out = 0;
        return;
    }

    // Build (mu, sigma) point cloud from shells[1..] (excludes shell[0])
    vector<pair<double,double>> pts;
    for (int i = 1; i < nShells; ++i) {
        if (shells_out[i].n_vertices > 0)
            pts.emplace_back(shells_out[i].mu, shells_out[i].sigma);
    }

    if (static_cast<int>(pts.size()) < 4) {
        // Too few points for covariance — fallback: peel at shell[1]
        peel_index_out = 1;
        return;
    }

    const double np = static_cast<double>(pts.size());
    double meanMu = 0, meanSig = 0;
    for (auto& [mu, sig] : pts) { meanMu += mu; meanSig += sig; }
    meanMu /= np; meanSig /= np;

    double c00 = 0, c01 = 0, c11 = 0;
    for (auto& [mu, sig] : pts) {
        double dm = mu - meanMu, ds = sig - meanSig;
        c00 += dm*dm; c01 += dm*ds; c11 += ds*ds;
    }
    const double div = np - 1.0;
    double cov[2][2] = {{c00/div, c01/div}, {c01/div, c11/div}};

    // 2×2 matrix inversion
    double det = cov[0][0]*cov[1][1] - cov[0][1]*cov[1][0];
    if (std::abs(det) < 1e-10) {
        peel_index_out = 1;
        return;
    }
    double inv[2][2];
    inv[0][0] =  cov[1][1] / det;
    inv[0][1] = -cov[0][1] / det;
    inv[1][0] = -cov[1][0] / det;
    inv[1][1] =  cov[0][0] / det;

    // Compute Mahalanobis for each shell[1..]
    // pts[i] corresponds to shells_out[i+1] (offset = 1 for EXCLUDE_OUTERMOST_SHELL)
    vector<bool> inside(pts.size(), false);
    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
        double diff[2] = {pts[i].first - meanMu, pts[i].second - meanSig};
        double t0 = inv[0][0]*diff[0] + inv[0][1]*diff[1];
        double t1 = inv[1][0]*diff[0] + inv[1][1]*diff[1];
        double d  = std::sqrt(diff[0]*t0 + diff[1]*t1);

        int shellIdx = i + 1;  // offset = 1 (EXCLUDE_OUTERMOST_SHELL)
        if (shellIdx < nShells) mahal_out[shellIdx] = d;
        inside[i] = (d <= SIGMA_THRESHOLD);
    }

    // Find first shell inside the 3-sigma ellipse → that is the cut point
    // (mirrors cutting3SigmaDistance logic: return i-1+offset for first inside[i])
    const int offset = 1;  // EXCLUDE_OUTERMOST_SHELL
    for (int i = 0; i < static_cast<int>(inside.size()); ++i) {
        if (inside[i]) {
            if (i == 0) {
                // First stats-point is already inside — distance jump fallback
                // (simplified: return shell[1])
                peel_index_out = offset;
                return;
            }
            peel_index_out = (i - 1) + offset;
            return;
        }
    }

    // All outside → distance jump (simplified: max consecutive distance)
    vector<double> dists;
    for (int i = 0; i + 1 < static_cast<int>(pts.size()); ++i) {
        double dmu = pts[i+1].first  - pts[i].first;
        double ds  = pts[i+1].second - pts[i].second;
        dists.push_back(std::sqrt(dmu*dmu + ds*ds));
    }
    if (dists.empty()) {
        peel_index_out = offset;
        return;
    }
    double dmean = 0.0;
    for (double d : dists) dmean += d;
    dmean /= static_cast<double>(dists.size());
    double dvar = 0.0;
    for (double d : dists) dvar += (d - dmean)*(d - dmean);
    double dstd = std::sqrt(dvar / static_cast<double>(dists.size()));
    double threshold = dmean + DISTANCE_JUMP_SIGMA * dstd;

    for (int i = 0; i < static_cast<int>(dists.size()); ++i) {
        if (dists[i] > threshold) {
            peel_index_out = i + offset;
            return;
        }
    }
    peel_index_out = offset;
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc < 3) {
        cerr << "Usage: " << argv[0]
             << " <dicom_dir> <output_csv> [beta_min=0.50] [beta_max=0.90] [beta_step=0.01]\n";
        return 1;
    }

    const string dicom_dir  = argv[1];
    const string output_csv = argv[2];
    const double beta_min   = (argc > 3) ? stod(argv[3]) : 0.50;
    const double beta_max   = (argc > 4) ? stod(argv[4]) : 0.90;
    const double beta_step  = (argc > 5) ? stod(argv[5]) : 0.01;

    if (beta_step <= 0.0 || beta_min > beta_max) {
        cerr << "Invalid beta range / step.\n";
        return 1;
    }

    // Shells output path: <output_csv>.shells.csv
    const string shells_csv_path = output_csv + ".shells.csv";

    // -------------------------------------------------------------------------
    // Discover DICOM files
    // -------------------------------------------------------------------------
    cerr << "[exp_beta_sweep] Discovering DICOM files in: " << dicom_dir << "\n";
    vector<string> files;
    try {
        files = findDicomFiles(dicom_dir);
    } catch (const exception& e) {
        cerr << "[exp_beta_sweep] Directory error: " << e.what() << "\n";
        return 1;
    }
    if (files.empty()) {
        cerr << "[exp_beta_sweep] No .dcm files found.\n";
        return 1;
    }
    cerr << "[exp_beta_sweep] Found " << files.size() << " file(s).\n";

    // -------------------------------------------------------------------------
    // Load all slices
    // -------------------------------------------------------------------------
    cerr << "[exp_beta_sweep] Loading slices...\n";
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
        cerr << "[exp_beta_sweep] No slices loaded successfully.\n";
        return 1;
    }

    // Cross-slice normalization (matches GUI pipeline)
    normalizeIntensities(images);
    cerr << "[exp_beta_sweep] Loaded and normalized " << images.size() << " slice(s).\n";

    // -------------------------------------------------------------------------
    // Open output CSVs
    // -------------------------------------------------------------------------
    ofstream csv(output_csv);
    if (!csv.is_open()) {
        cerr << "[exp_beta_sweep] Cannot open output: " << output_csv << "\n";
        return 1;
    }
    ofstream shells_csv(shells_csv_path);
    if (!shells_csv.is_open()) {
        cerr << "[exp_beta_sweep] Cannot open shells output: " << shells_csv_path << "\n";
        return 1;
    }

    csv << fixed << setprecision(6);
    // Component CSV header — extended with shell summary columns
    csv << "slice,beta,n_components,n_faces,area,perimeter,circularity,score,"
           "n_inner_loops,n_outer_edges,outer_circularity,"
           "peel_index,n_shells,mahal_d_at_cut\n";

    shells_csv << fixed << setprecision(6);
    // Per-shell CSV header
    shells_csv << "slice,beta,shell_index,mu,sigma,n_vertices,mahal_d\n";

    // -------------------------------------------------------------------------
    // Beta sweep
    // -------------------------------------------------------------------------
    for (size_t idx = 0; idx < images.size(); ++idx) {
        const CTImage& img = *images[idx];
        string slice_name = fs::path(img.filepath()).filename().string();

        cerr << "[" << (idx + 1) << "/" << images.size() << "] " << slice_name << "\n";

        int W = img.width(), H = img.height();
        const auto& pixels = img.image();

        // Column-major dataset for the V library (matches CTImage::calculateShells)
        vector<vector<int>> colorDataSet(W, vector<int>(H));
        for (int r = 0; r < H; ++r)
            for (int c = 0; c < W; ++c)
                colorDataSet[c][r] = static_cast<int>(pixels[r * W + c]);

        // Build Voronoi ONCE for this slice
        ImageProcessingBC bc;
        try {
            bc.constructForImageProcessing16bit(colorDataSet, IMAGE_BIT_DEPTH,
                                                DARKER_PIXEL_BIGGER_DISK);
        } catch (const exception& e) {
            cerr << "  Voronoi construction failed: " << e.what() << " — skipping.\n";
            continue;
        }

        // Sweep beta values
        for (double beta = beta_min; beta <= beta_max + 1e-9; beta += beta_step) {
            double beta_r = round(beta * 1000.0) / 1000.0;  // avoid fp drift

            bc.set_beta_value(beta_r);
            bc.classifyByComponents();
            bc.select_biggest_componentBSFace();  // select via score without building edge list

            // Read metrics directly from ComponentBSFace (library-consistent values)
            int    n_comp    = bc.get_n_components();
            int    n_faces   = bc.get_n_beta_faces_of_biggest_component();
            double area      = bc.get_area_of_biggest_component();
            double circ      = bc.get_circularity_of_biggest_component();
            double score     = area * circ * circ;
            // Back-calculate perimeter from circularity = 2*sqrt(pi*A)/P
            double peri      = (circ > 1e-9) ? (2.0 * sqrt(M_PI * area) / circ) : 0.0;
            int    n_inner     = bc.get_n_inner_loops_of_biggest_component();
            int    n_outer_e   = bc.get_n_outer_edges_of_biggest_component();
            double outer_circ  = bc.get_outer_circularity_of_biggest_component();

            // ------------------------------------------------------------------
            // Shell statistics (same Voronoi, new traversal per β)
            // reset_vertex_visited() clears vertex flags left from any prior β's
            // shell traversal so the outer-loop + inner-shell extraction is clean.
            // ------------------------------------------------------------------
            vector<ShellData> shells;
            int    peel_idx      = 0;
            vector<double> mahal;

            try {
                bc.reset_vertex_visited();
                compute_shell_stats_inline(bc, beta_r, shells, peel_idx, mahal);
            } catch (const exception& e) {
                cerr << "  Shell stats failed for " << slice_name
                     << " beta=" << beta_r << ": " << e.what() << "\n";
                // leave shells empty — write sentinel values
            }

            int    n_shells       = static_cast<int>(shells.size());
            double mahal_at_cut   = -1.0;
            if (peel_idx >= 0 && peel_idx < static_cast<int>(mahal.size()))
                mahal_at_cut = mahal[peel_idx];

            // Component CSV row (original 11 columns + 3 shell summary columns)
            csv << slice_name << ","
                << beta_r    << ","
                << n_comp    << ","
                << n_faces   << ","
                << area      << ","
                << peri      << ","
                << circ      << ","
                << score     << ","
                << n_inner   << ","
                << n_outer_e << ","
                << outer_circ << ","
                << peel_idx  << ","
                << n_shells  << ","
                << mahal_at_cut << "\n";

            // Per-shell CSV rows
            for (int si = 0; si < n_shells; ++si) {
                double md = (si < static_cast<int>(mahal.size())) ? mahal[si] : -1.0;
                shells_csv << slice_name << ","
                           << beta_r    << ","
                           << si        << ","
                           << shells[si].mu     << ","
                           << shells[si].sigma  << ","
                           << shells[si].n_vertices << ","
                           << md        << "\n";
            }
        }
    }

    csv.close();
    shells_csv.close();
    cerr << "[exp_beta_sweep] Done.\n";
    cerr << "  Component CSV : " << output_csv      << "\n";
    cerr << "  Shells CSV    : " << shells_csv_path << "\n";
    return 0;
}

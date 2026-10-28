// =============================================================================
// exp_shell_export — Shell statistics export for SKINPEELER paper
//
// Loads DICOM slices, applies cross-slice normalization (same as GUI pipeline),
// then runs calculateShells() at a fixed beta value and
// exports per-shell statistics for every slice.
//
// Each row corresponds to one shell of one slice. The skin_cut_index column
// records the cutting point as determined by the 3-sigma algorithm
// (CTImage::peelIndex(), which reflects calculateCuttingShell() result).
//
// CSV columns (7):
//   slice, shell_index, mu, sigma, n_vertices, skin_cut_index, n_shells
//
// Usage:
//   exp_shell_export <dicom_dir> <output_csv> [beta=0.60] [max_shells=30]
//
// Notes:
//   * max_shells parameter is informational only — actual shell count is
//     determined by ADAPTIVE_SHELL_MAX in Settings.h and available topology.
//     maximizeShells=true disables early-stopping, extracting as many as possible.
//   * Cross-slice normalization: global max across all loaded slices applied via
//     normalizeToMax(globalMax), matching DicomService::normalizeIntensities().
//   * Memory note: Voronoi construction requires ~500 MB per slice. All slices
//     are loaded first (pixel data only), then processed one at a time. Voronoi
//     is released after each slice via releaseVoronoi().
// =============================================================================

#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <filesystem>

// SKINPEELER models
#include "models/CTImage.h"
#include "models/CTShell.h"
#include "config/Settings.h"

namespace fs = std::filesystem;
using namespace std;

// -----------------------------------------------------------------------------
// File discovery: find and sort .dcm files by trailing number
// (identical to exp_beta_sweep)
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

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc < 3) {
        cerr << "Usage: " << argv[0]
             << " <dicom_dir> <output_csv> [beta=0.60] [max_shells=30]\n";
        return 1;
    }

    const string dicom_dir  = argv[1];
    const string output_csv = argv[2];
    const double beta       = (argc > 3) ? stod(argv[3]) : DEFAULT_BETA;
    // max_shells is informational; actual limit comes from ADAPTIVE_SHELL_MAX
    const int    max_shells = (argc > 4) ? stoi(argv[4]) : 30;
    (void)max_shells; // used only for user-facing documentation

    if (beta <= 0.0 || beta >= 1.0) {
        cerr << "[exp_shell_export] beta must be in (0, 1). Got: " << beta << "\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // Discover DICOM files
    // -------------------------------------------------------------------------
    cerr << "[exp_shell_export] Discovering DICOM files in: " << dicom_dir << "\n";
    vector<string> files;
    try {
        files = findDicomFiles(dicom_dir);
    } catch (const exception& e) {
        cerr << "[exp_shell_export] Directory error: " << e.what() << "\n";
        return 1;
    }
    if (files.empty()) {
        cerr << "[exp_shell_export] No .dcm files found.\n";
        return 1;
    }
    cerr << "[exp_shell_export] Found " << files.size() << " file(s).\n";

    // -------------------------------------------------------------------------
    // Load all slices (pixel data only; Voronoi not yet built)
    // -------------------------------------------------------------------------
    cerr << "[exp_shell_export] Loading slices...\n";
    vector<unique_ptr<CTImage>> images;
    images.reserve(files.size());
    for (const auto& path : files) {
        try {
            images.push_back(make_unique<CTImage>(
                CTImage::fromDicomFile(path, beta, DEFAULT_SHELL_THICKNESS)));
        } catch (const exception& e) {
            cerr << "  Skipping " << fs::path(path).filename().string()
                 << ": " << e.what() << "\n";
        }
    }
    if (images.empty()) {
        cerr << "[exp_shell_export] No slices loaded successfully.\n";
        return 1;
    }

    // Cross-slice normalization (matches GUI pipeline)
    normalizeIntensities(images);
    cerr << "[exp_shell_export] Loaded and normalized " << images.size()
         << " slice(s). beta=" << beta << "\n";

    // -------------------------------------------------------------------------
    // Open output CSV
    // -------------------------------------------------------------------------
    ofstream csv(output_csv);
    if (!csv.is_open()) {
        cerr << "[exp_shell_export] Cannot open output: " << output_csv << "\n";
        return 1;
    }
    csv << fixed << setprecision(6);
    csv << "slice,shell_index,mu,sigma,n_vertices,skin_cut_index,n_shells,mahal_d,inside_3sig\n";

    // -------------------------------------------------------------------------
    // Per-slice: build Voronoi, extract shells, write rows
    // -------------------------------------------------------------------------
    int successCount = 0;
    int failCount    = 0;

    for (size_t idx = 0; idx < images.size(); ++idx) {
        CTImage& img = *images[idx];
        string slice_name = fs::path(img.filepath()).filename().string();

        cerr << "[" << (idx + 1) << "/" << images.size() << "] " << slice_name << "\n";

        // Build Voronoi + extract all shells
        try {
            img.calculateShells();
        } catch (const exception& e) {
            cerr << "  Shell extraction failed: " << e.what() << " — skipping.\n";
            ++failCount;
            continue;
        }

        const auto& shells  = img.shells();
        const int   n_shells   = static_cast<int>(shells.size());
        const int   cut_index  = img.peelIndex();  // cutting3SigmaDistance result

        if (n_shells == 0) {
            cerr << "  No shells extracted — skipping.\n";
            ++failCount;
            continue;
        }

        const auto& mahalD  = img.shellMahalD();
        const auto& insideV = img.shellInside();

        for (int s = 0; s < n_shells; ++s) {
            const CTShell& shell = shells[s];
            const double   mu        = shell.hasStatistics() ? shell.mu()    : 0.0;
            const double   sigma     = shell.hasStatistics() ? shell.sigma() : 0.0;
            const int      n_verts   = static_cast<int>(shell.size());
            const double   md        = (s < (int)mahalD.size())  ? mahalD[s]  : -1.0;
            const int      ins       = (s < (int)insideV.size()) ? (insideV[s] ? 1 : 0) : 0;

            csv << slice_name << ","
                << s          << ","
                << mu         << ","
                << sigma      << ","
                << n_verts    << ","
                << cut_index  << ","
                << n_shells   << ","
                << md         << ","
                << ins        << "\n";
        }

        // Release Voronoi memory before moving to next slice (~500 MB freed)
        img.releaseVoronoi();
        ++successCount;
    }

    csv.close();

    cerr << "[exp_shell_export] Done.\n"
         << "  Processed: " << successCount << " slice(s)\n"
         << "  Failed:    " << failCount    << " slice(s)\n"
         << "  Output:    " << output_csv   << "\n";

    return (successCount > 0) ? 0 : 1;
}

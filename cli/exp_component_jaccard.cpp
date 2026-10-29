// =============================================================================
// exp_component_jaccard — Cut-independent inter-slice Jaccard (F3 fast path)
//
// Computes Jaccard similarity between the biggest beta-complex component
// (body mask) of adjacent CT slices, entirely in C++.  This is a
// cut-independent topological continuity metric: no inner-shell peeling,
// no Mahalanobis classification, no bed detection — just VD construction +
// classifyByComponents + select_biggest, then pixel-mask Jaccard.
//
// The body mask is obtained via CTImage::computeBiggestComponentMask(), which
// wraps ImageProcessingBC::biggest_component_pixel_mask().  Voronoi is
// released immediately after each slice to bound peak RSS.  Only prev/curr
// two masks are kept in memory at any time.
//
// Slice ordering: numeric suffix extracted from filename via _CT_(\d+)
// regex, matching Python run_interslice_jaccard.py extract_slice_num().
//
// Usage:
//   exp_component_jaccard <dicom_dir> <specimen_label> <output_csv> [beta=0.69] [max_slices=0]
//
//   max_slices: if > 0, stop after this many slices (sanity mode).
//               Use 20 for a quick area/Jaccard sanity check before full run.
//
// CSV columns:
//   specimen, slice_num, filename, jaccard_with_prev, area_px_curr
//
//   jaccard_with_prev: NaN for the first slice; Jaccard(prev,curr) otherwise.
//   area_px_curr:      number of body pixels in the current slice mask.
//
// Build: see CMakeLists.txt target exp_component_jaccard.
// =============================================================================

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>
#include <utility>

// SKINPEELER engine
#include "models/CTImage.h"
#include "config/Settings.h"
#include "ImageProcessingBC.h"   // for the static circularity-metric switch (legacy control)

namespace fs = std::filesystem;

// =============================================================================
// Slice number extraction
// Matches Python extract_slice_num: finds _CT_(\d+) in filename, else any (\d+).
// =============================================================================
static int extractSliceNum(const std::string& path)
{
    std::string name = fs::path(path).filename().string();
    std::smatch m;
    // Primary pattern: _CT_NNN (CB20/GC20/MM20 convention)
    static const std::regex rePrimary(R"(_CT_(\d+))");
    if (std::regex_search(name, m, rePrimary))
        return std::stoi(m[1].str());
    // Fallback: first numeric run
    static const std::regex reFallback(R"((\d+))");
    if (std::regex_search(name, m, reFallback))
        return std::stoi(m[1].str());
    return 0;
}

// =============================================================================
// Discover and sort .dcm files by numeric slice number
// =============================================================================
static std::vector<std::string> findAndSortDicom(const std::string& dir)
{
    std::vector<std::pair<int, std::string>> keyed;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".dcm") continue;
        std::string p = entry.path().string();
        keyed.emplace_back(extractSliceNum(p), p);
    }
    std::sort(keyed.begin(), keyed.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<std::string> result;
    result.reserve(keyed.size());
    for (auto& [num, p] : keyed)
        result.push_back(p);
    return result;
}

// =============================================================================
// Jaccard of two flat char masks (1=body, 0=bg), same length
// =============================================================================
static double jaccardMasks(const std::vector<char>& a, const std::vector<char>& b)
{
    assert(a.size() == b.size());
    long long intersection = 0, union_count = 0;
    const size_t N = a.size();
    for (size_t i = 0; i < N; ++i) {
        const bool ab = (a[i] != 0), bb = (b[i] != 0);
        if (ab && bb) ++intersection;
        if (ab || bb) ++union_count;
    }
    if (union_count == 0) return std::numeric_limits<double>::quiet_NaN();
    return static_cast<double>(intersection) / static_cast<double>(union_count);
}

// =============================================================================
// Count body pixels in mask
// =============================================================================
static long long countBody(const std::vector<char>& mask)
{
    long long n = 0;
    for (char c : mask) if (c) ++n;
    return n;
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[])
{
    if (argc < 4) {
        std::cerr << "Usage: exp_component_jaccard <dicom_dir> <specimen_label>"
                     " <output_csv> [beta=0.69] [max_slices=0]\n";
        return 1;
    }

    const std::string dicomDir   = argv[1];
    const std::string specimen   = argv[2];
    const std::string outputCsv  = argv[3];
    const double      beta       = (argc >= 5) ? std::stod(argv[4]) : 0.69;
    const int         maxSlices  = (argc >= 6) ? std::stoi(argv[5]) : 0;

    // Legacy control: SKINPEELER_LEGACY_CIRC=1 reverts the selection score to the
    // total-perimeter (outer+inner-hole) circularity. Default = outer-loop only.
    bool legacyCirc = false;
    if (const char* e = std::getenv("SKINPEELER_LEGACY_CIRC")) {
        if (std::string(e) == "1") { V::ImageProcessingBC::set_use_outer_loop_circularity(false); legacyCirc = true; }
    }
    std::cout << "  circularity: " << (legacyCirc ? "LEGACY total-perimeter" : "outer-loop only") << "\n";

    std::cout << "=== exp_component_jaccard ===\n";
    std::cout << "  dicom_dir  : " << dicomDir << "\n";
    std::cout << "  specimen   : " << specimen  << "\n";
    std::cout << "  output_csv : " << outputCsv << "\n";
    std::cout << "  beta       : " << std::fixed << std::setprecision(4) << beta << "\n";
    std::cout << "  max_slices : " << (maxSlices > 0 ? std::to_string(maxSlices) : "all") << "\n";

    // Discover DICOM files
    auto paths = findAndSortDicom(dicomDir);
    if (paths.empty()) {
        std::cerr << "ERROR: no .dcm files found in " << dicomDir << "\n";
        return 1;
    }
    std::cout << "  found      : " << paths.size() << " .dcm files\n\n";

    // Apply slice limit for sanity mode
    const int total = (maxSlices > 0 && maxSlices < static_cast<int>(paths.size()))
                      ? maxSlices
                      : static_cast<int>(paths.size());

    // Open output CSV
    fs::create_directories(fs::path(outputCsv).parent_path());
    std::ofstream csv(outputCsv);
    if (!csv.is_open()) {
        std::cerr << "ERROR: cannot open output CSV: " << outputCsv << "\n";
        return 1;
    }
    csv << "specimen,slice_num,filename,jaccard_with_prev,area_px_curr\n";
    csv << std::fixed << std::setprecision(6);

    // Per-slice processing — prev/curr two masks only in memory
    std::vector<char> prevMask;
    int prevSliceNum = -1;

    int nSuccess = 0, nFail = 0;

    for (int i = 0; i < total; ++i) {
        const std::string& fpath = paths[static_cast<size_t>(i)];
        const std::string  fname = fs::path(fpath).filename().string();
        const int          snum  = extractSliceNum(fpath);

        std::cout << "[" << (i + 1) << "/" << total << "] "
                  << fname << " (slice_num=" << snum << ") ... " << std::flush;

        std::vector<char> currMask;
        bool loadOk = false;

        try {
            // Load DICOM + build VD + get component mask (VD released inside)
            CTImage img = CTImage::fromDicomFile(fpath, beta);
            img.releaseRawDicom();          // free ~1.5 MB raw buffer (no export needed)
            currMask = img.computeBiggestComponentMask();
            loadOk = true;
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << e.what() << "\n";
            ++nFail;
            prevMask.clear();   // gap — next slice cannot compare
            csv << specimen << "," << snum << "," << fname << ",NaN,0\n";
            continue;
        }

        long long area = countBody(currMask);
        std::cout << "area=" << area;

        double jac = std::numeric_limits<double>::quiet_NaN();
        if (!prevMask.empty() && prevMask.size() == currMask.size()) {
            jac = jaccardMasks(prevMask, currMask);
            std::cout << " J=" << std::fixed << std::setprecision(4) << jac;
        } else {
            std::cout << " J=NaN(first)";
        }
        std::cout << "\n";

        // Write CSV row
        csv << specimen << "," << snum << "," << fname << ",";
        if (std::isnan(jac))
            csv << "NaN";
        else
            csv << jac;
        csv << "," << area << "\n";

        prevMask     = std::move(currMask);
        prevSliceNum = snum;
        ++nSuccess;
    }

    csv.close();

    std::cout << "\n=== Summary ===\n";
    std::cout << "  processed : " << nSuccess << " ok, " << nFail << " failed\n";
    std::cout << "  output    : " << outputCsv << "\n";

    return (nFail > 0) ? 2 : 0;
}

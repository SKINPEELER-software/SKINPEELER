// =============================================================================
// exp_shell_pixel_export — Per-pixel shell index export for SKINPEELER paper
//
// Loads a single DICOM slice (or directory of .dcm files), runs the full
// SKINPEELER pipeline (Voronoi/QT/BC + calculateShells()), then produces an
// indexed 16-bit grayscale PNG where each pixel value equals the shell_index
// of the shell whose Voronoi vertex disk covers that pixel.
//
// Pixel assignment strategy  (matches peelSkin() rasterization):
//   For each shell[s], iterate vertices.  Convert (vx, vy) → (row, col) via
//   CoordinateTransform::voronoiToImagePixel().  Write shell_index = s+1 (1-based
//   so that 0 = unassigned/outside).  Shells are painted outermost→innermost,
//   so inner shells overwrite outer ones — the final map reflects the innermost
//   covering shell at every pixel.
//
// Output:
//   16-bit single-channel PNG (stb_image_write, STBIW_WINDOWS_UTF8 not needed).
//   Pixel value : 0  = outside/air (no shell vertex covers this pixel)
//                 1  = shell[0]  (outermost)
//                 2  = shell[1]
//                 …
//                 N  = shell[N-1]
//
// File naming:
//   <output_dir>/<dicom_stem>_b<beta3digits>_shell.png
//   e.g. 199_CT_378_D1_b069_shell.png
//
// CLI:
//   exp_shell_pixel_export <dicom_file_or_dir> <output_dir> [beta=0.70]
//
// Notes:
//   * Single-slice normalization: globalMax = imageMax() of that slice.
//     This differs from cross-slice norm used by exp_shell_export.
//     Rationale: single-slice visual comparison does not need cross-slice
//     consistency; shell topology depends only on relative intensities within
//     the slice.
//   * Memory: same as other experiment binaries (~500 MB peak for Voronoi).
//     releaseVoronoi() called after shell extraction.
//   * max shell_index for current specimens is ~30 << 65535, so 16-bit is safe.
//   * stb_image_write.h is header-only; define STB_IMAGE_WRITE_IMPLEMENTATION
//     in exactly one .cpp translation unit (this file).
// =============================================================================

// stb_image_write — must define implementation exactly once
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <chrono>
#include <set>

// SKINPEELER models
#include "models/CTImage.h"
#include "models/CTShell.h"
#include "config/Settings.h"
#include "utils/CoordinateTransform.h"

namespace fs = std::filesystem;
using namespace std;

// =============================================================================
// File discovery helpers (identical pattern to other experiment binaries)
// =============================================================================
static int extract_file_number(const string& path)
{
    string name = fs::path(path).filename().string();
    size_t dot  = name.rfind('.');
    if (dot == string::npos) return 0;
    size_t end   = dot;
    size_t start = end;
    while (start > 0 && isdigit(static_cast<unsigned char>(name[start - 1])))
        --start;
    if (start == end) return 0;
    return stoi(name.substr(start, end - start));
}

static vector<string> find_dicom_files(const string& directory)
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
            return extract_file_number(a) < extract_file_number(b);
        });
    return paths;
}

// =============================================================================
// Beta → 3-digit string  (e.g. 0.69 → "069",  0.70 → "070")
// =============================================================================
static string beta_tag(double beta)
{
    // Round to 2 decimal places, then express as 3 chars: "0.69" → "069"
    int v = static_cast<int>(round(beta * 100.0));
    ostringstream oss;
    oss << setw(3) << setfill('0') << v;
    return oss.str();
}

// =============================================================================
// Build output PNG path from input DICOM stem + beta
// <output_dir>/<stem>_b<beta3>_shell.png
// =============================================================================
static string make_output_path(const string& dicom_path,
                               const string& output_dir,
                               double        beta)
{
    string stem = fs::path(dicom_path).stem().string();   // no extension
    string fname = stem + "_b" + beta_tag(beta) + "_shell.png";
    return (fs::path(output_dir) / fname).string();
}

// =============================================================================
// Build the shell-index pixel map and write PNG
//
// Algorithm:
//   1. Allocate uint16_t buffer (height x width), init to 0 (outside).
//   2. For shell index s = 0 .. n_shells-1  (outermost first):
//        for each vertex v in shell[s]:
//          (row, col) = voronoiToImagePixel(v.x, v.y, height, width)
//          if valid: buf[row*width + col] = s + 1   (1-based)
//      Because we iterate shells outermost→innermost and overwrite, inner
//      shells take priority at contested pixels.
//   3. stbi_write_png_16 writes the 16-bit grayscale PNG.
//
// Returns: sorted list of unique shell_index values present in the image.
// =============================================================================
static vector<uint8_t> write_shell_png(const CTImage&  img,
                                        const string&   out_path)
{
    const int w = img.width();
    const int h = img.height();
    const auto& shells = img.shells();
    const int n_shells = static_cast<int>(shells.size());

    if (n_shells > 254)
        throw runtime_error("shell count exceeds uint8 range (255 max)");

    // 8-bit single channel (shell index <= 254)
    vector<uint8_t> buf(static_cast<size_t>(h) * static_cast<size_t>(w), 0u);

    // Paint shells outermost → innermost (innermost wins on overlap)
    for (int s = 0; s < n_shells; ++s) {
        const CTShell& shell = shells[s];
        const uint8_t label = static_cast<uint8_t>(s + 1);  // 1-based

        for (const auto& v : shell) {
            auto [row, col] = CoordinateTransform::voronoiToImagePixel(
                v.x, v.y, h, w);
            if (!CoordinateTransform::isValidPixel(row, col, h, w))
                continue;
            buf[static_cast<size_t>(row) * static_cast<size_t>(w) + static_cast<size_t>(col)] = label;
        }
    }

    int rc = stbi_write_png(
        out_path.c_str(),
        w, h,
        1,
        buf.data(),
        w);

    if (rc == 0)
        throw runtime_error("stbi_write_png failed for: " + out_path);

    set<uint8_t> uniq(buf.begin(), buf.end());
    return vector<uint8_t>(uniq.begin(), uniq.end());
}

// =============================================================================
// Process one DICOM file → one PNG
// =============================================================================
static bool process_single(const string& dicom_path,
                            const string& output_dir,
                            double        beta)
{
    string stem = fs::path(dicom_path).stem().string();
    string out_path = make_output_path(dicom_path, output_dir, beta);

    cerr << "[exp_shell_pixel_export] Processing: "
         << fs::path(dicom_path).filename().string()
         << "  beta=" << beta
         << "  → " << fs::path(out_path).filename().string() << "\n";

    auto t0 = chrono::steady_clock::now();

    // 1. Load DICOM
    CTImage img = CTImage::fromDicomFile(dicom_path, beta, DEFAULT_SHELL_THICKNESS);

    // 2. Single-slice normalization: normalizeToMax(imageMax)
    //    Rationale: this binary targets single-slice visualization.
    //    Cross-slice norm is not meaningful for a standalone PNG export.
    double local_max = img.imageMax();
    if (local_max <= 0.0) local_max = 1.0;
    img.normalizeToMax(local_max);

    // 3. Build Voronoi + extract all shells
    try {
        img.calculateShells();
    } catch (const exception& e) {
        cerr << "  Shell extraction failed: " << e.what() << "\n";
        return false;
    }

    const int n_shells = static_cast<int>(img.shells().size());
    if (n_shells == 0) {
        cerr << "  No shells extracted — skipping.\n";
        return false;
    }

    const int cut_idx = img.peelIndex();
    cerr << "  Shells extracted: " << n_shells
         << "  cut_index=" << cut_idx << "\n";

    // 4. Write shell-index PNG
    vector<uint8_t> unique_vals;
    try {
        unique_vals = write_shell_png(img, out_path);
    } catch (const exception& e) {
        cerr << "  PNG write failed: " << e.what() << "\n";
        return false;
    }

    // 5. Release Voronoi memory (~500 MB)
    img.releaseVoronoi();

    auto t1 = chrono::steady_clock::now();
    double elapsed = chrono::duration<double>(t1 - t0).count();

    // Report unique shell_index values (exclude 0 = unassigned)
    cerr << "  Unique shell indices in PNG: [";
    bool first = true;
    for (auto v : unique_vals) {
        if (!first) cerr << ", ";
        cerr << v;
        first = false;
    }
    cerr << "]\n";
    cerr << "  Written: " << out_path << "  (" << fixed << setprecision(1)
         << elapsed << "s)\n";

    // Also print to stdout for caller scripting
    cout << out_path;
    cout << "\tunique_shell_indices=";
    first = true;
    for (auto v : unique_vals) {
        if (!first) cout << ",";
        cout << v;
        first = false;
    }
    cout << "\tn_shells=" << n_shells
         << "\tcut_index=" << cut_idx
         << "\tbeta=" << beta
         << "\tslice=" << stem
         << "\n";

    return true;
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[])
{
    if (argc < 3) {
        cerr << "Usage: " << argv[0]
             << " <dicom_file_or_dir> <output_dir> [beta=0.70]\n"
             << "\n"
             << "  dicom_file_or_dir  single .dcm file OR directory of .dcm files\n"
             << "  output_dir         directory where PNG(s) will be written\n"
             << "  beta               beta parameter (default=" << DEFAULT_BETA << ")\n"
             << "\n"
             << "  Output file naming: <stem>_b<beta3digits>_shell.png\n"
             << "  PNG pixel value:    0=outside, 1=shell[0](outermost), 2=shell[1], ...\n";
        return 1;
    }

    const string input_path = argv[1];
    const string output_dir = argv[2];
    const double beta       = (argc > 3) ? stod(argv[3]) : DEFAULT_BETA;

    if (beta <= 0.0 || beta >= 1.0) {
        cerr << "[exp_shell_pixel_export] beta must be in (0,1). Got: " << beta << "\n";
        return 1;
    }

    // Ensure output directory exists
    {
        error_code ec;
        fs::create_directories(output_dir, ec);
        if (ec) {
            cerr << "[exp_shell_pixel_export] Cannot create output dir: "
                 << output_dir << " : " << ec.message() << "\n";
            return 1;
        }
    }

    // Collect input files
    vector<string> dicom_files;
    if (fs::is_directory(input_path)) {
        dicom_files = find_dicom_files(input_path);
        if (dicom_files.empty()) {
            cerr << "[exp_shell_pixel_export] No .dcm files found in: " << input_path << "\n";
            return 1;
        }
        cerr << "[exp_shell_pixel_export] Directory mode: "
             << dicom_files.size() << " file(s) found.\n";
    } else {
        // Single file
        string ext = fs::path(input_path).extension().string();
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".dcm") {
            cerr << "[exp_shell_pixel_export] Input is not a .dcm file: " << input_path << "\n";
            return 1;
        }
        dicom_files.push_back(input_path);
    }

    cerr << "[exp_shell_pixel_export] beta=" << beta
         << "  output_dir=" << output_dir << "\n";

    auto t_total_start = chrono::steady_clock::now();

    int success_count = 0;
    int fail_count    = 0;

    for (size_t i = 0; i < dicom_files.size(); ++i) {
        cerr << "\n[" << (i + 1) << "/" << dicom_files.size() << "] ";
        bool ok = process_single(dicom_files[i], output_dir, beta);
        if (ok) ++success_count;
        else    ++fail_count;
    }

    auto t_total_end = chrono::steady_clock::now();
    double total_sec = chrono::duration<double>(t_total_end - t_total_start).count();

    cerr << "\n[exp_shell_pixel_export] Summary\n"
         << "  Processed: " << success_count << "\n"
         << "  Failed:    " << fail_count    << "\n"
         << "  Total time: " << fixed << setprecision(1) << total_sec << "s\n";

    return (success_count > 0) ? 0 : 1;
}

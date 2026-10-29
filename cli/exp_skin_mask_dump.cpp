// =============================================================================
// exp_skin_mask_dump — CT intensity + skin segmentation mask + component mask dump
//
// For each specified (specimen, slice_num) target, runs the full SKINPEELER
// pipeline (P2D → QT → β-complex → biggest component → shell peeling → cut)
// and writes three binary output files per slice:
//
//   <specimen>_slice<NNNN>_ct.raw   — windowed CT intensity (uint16, row-major,
//                                     512×512, little-endian)
//   <specimen>_slice<NNNN>_mask.raw — skin mask (uint8, row-major, 512×512;
//                                     1 = skin pixel, 0 = air/body/background)
//   <specimen>_slice<NNNN>_comp.raw — biggest β-complex component pixel mask
//                                     (uint8, row-major, 512×512;
//                                      1 = component pixel, 0 = background)
//                                     cut-independent, pre-peel, Jaccard target.
//   <specimen>_slice<NNNN>_components.csv — per-component score diagnostics
//                                     (why a specific component was selected).
//
// Skin definition:
//   m_shells[0]                         = S₀ = air (outermost, excluded)
//   m_shells[1] .. m_shells[peelIndex]  = skin  → mask value 1
//   m_shells[peelIndex+1] ..            = body  → mask value 0
//
// Component mask definition:
//   Biggest β-complex connected component pixel set, extracted via
//   CTImage::extractBiggestComponentMaskFromVoronoi() after calculateShells()
//   and before releaseVoronoi().  Same coordinate system as skin mask.
//   Containment property: skin ⊆ component (skin pixels are a subset of
//   component pixels).
//
// CT intensity:
//   m_image after extractImageFromDicom() — windowed (WindowCenter/Width applied),
//   minimum-subtracted uint16 values exactly as the pipeline uses them.
//
// Output directory default: ./skin_mask_dump/
//   Override with --outdir <DIR> at end of CLI args.
//
// Usage (CLI mode — recommended):
//   exp_skin_mask_dump <dicom_dir> <specimen_label> <beta> <slice1> [slice2 ...] [--outdir <DIR>]
//
//   dicom_dir: path to directory containing .dcm files. Quote if it contains spaces.
//   specimen_label: short identifier for output filenames (e.g. PHANTOM).
//   beta: floating-point beta parameter (e.g. 0.69).
//   sliceN: one or more integer slice numbers to process.
//   --outdir <DIR>: (optional, must be last two args) override output directory.
//
//   Example:
//     exp_skin_mask_dump sample_data/phantom PHANTOM 0.69 1 4 8 --outdir /tmp/out
//
// Output format README:
//   *_ct.raw  : dtype=uint16 (little-endian), shape=(512,512), row-major
//   *_mask.raw: dtype=uint8,                  shape=(512,512), row-major
//   *_comp.raw: dtype=uint8,                  shape=(512,512), row-major
//   Read in Python:
//     ct   = np.fromfile("PHANTOM_slice0001_ct.raw",   dtype='<u2').reshape(512,512)
//     mask = np.fromfile("PHANTOM_slice0001_mask.raw",  dtype='u1').reshape(512,512)
//     comp = np.fromfile("PHANTOM_slice0001_comp.raw",  dtype='u1').reshape(512,512)
//
// Component CSV (*_components.csv):
//   Columns: component_id, centroid_x, centroid_y, area, circularity, score, is_selected
//   score = area * circularity^2 (same metric used for biggest-component selection).
//   is_selected = 1 for the component that was actually selected (highest score).
//   Rows sorted by score descending.
//
// Build: see CMakeLists.txt target exp_skin_mask_dump.
// =============================================================================

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <map>
#include <utility>

// SKINPEELER engine
#include "models/CTImage.h"
#include "models/CTShell.h"
#include "config/Settings.h"
#include "utils/CoordinateTransform.h"

namespace fs = std::filesystem;

// =============================================================================
// Slice number extraction  (mirrors exp_component_jaccard.cpp)
// =============================================================================
static int extractSliceNum(const std::string& path) {
    std::string name = fs::path(path).filename().string();
    std::smatch m;
    static const std::regex rePrimary(R"(_CT_(\d+))");
    if (std::regex_search(name, m, rePrimary))
        return std::stoi(m[1].str());
    static const std::regex reFallback(R"((\d+))");
    if (std::regex_search(name, m, reFallback))
        return std::stoi(m[1].str());
    return 0;
}

// =============================================================================
// Discover all .dcm files in a directory, build a map: slice_num → path
// =============================================================================
static std::map<int, std::string> buildSliceMap(const std::string& dir) {
    std::map<int, std::string> result;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".dcm")
            continue;
        std::string p = entry.path().string();
        int snum = extractSliceNum(p);
        result[snum] = p;
    }
    return result;
}

// =============================================================================
// Write a skin mask for the given CTImage (after calculateShells())
//
// Skin rule:
//   shells[0]               = S₀ = air → NOT skin (mask=0)
//   shells[1..peelIndex]    = skin     → mask=1
//   shells[peelIndex+1..]   = body     → mask=0
//   pixels not covered by any shell    → mask=0
//
// Returns the number of skin pixels (for sanity check).
// =============================================================================
static long long buildSkinMask(const CTImage& img, std::vector<uint8_t>& maskOut) {
    const int H = img.height();
    const int W = img.width();
    maskOut.assign(static_cast<size_t>(H * W), 0);

    const int peelIdx = img.peelIndex();
    const auto& shells = img.shells();
    const int nShells = static_cast<int>(shells.size());

    long long skinPixels = 0;

    // Iterate shells[1..peelIndex] (inclusive) — these are the skin shells.
    // shells[0] = S₀ = air → skip (start from si=1).
    for (int si = 1; si <= peelIdx && si < nShells; ++si) {
        for (const auto& vdata : shells[si]) {
            auto [row, col] = CoordinateTransform::voronoiToImagePixel(vdata.x, vdata.y, H, W);
            if (!CoordinateTransform::isValidPixel(row, col, H, W))
                continue;
            const int idx = row * W + col;
            if (maskOut[static_cast<size_t>(idx)] == 0) {
                maskOut[static_cast<size_t>(idx)] = 1;
                ++skinPixels;
            }
        }
    }
    return skinPixels;
}

// =============================================================================
// Write raw binary file
// =============================================================================
template <typename T>
static bool writeRaw(const std::string& path, const std::vector<T>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size() * sizeof(T)));
    return f.good();
}

// =============================================================================
// Write component score diagnostics CSV
//
// Columns: component_id, centroid_x, centroid_y, area, circularity, score, is_selected
// Rows sorted by score descending (index 0 = selected/highest).
// is_selected=1 for the row with the maximum score (the one the engine picked).
// =============================================================================
static bool writeComponentsCsv(const std::string& path,
                               const std::vector<CTImage::ComponentScoreInfo>& comps) {
    std::ofstream f(path);
    if (!f.is_open())
        return false;

    f << "component_id,centroid_x,centroid_y,area,circularity,score,is_selected,"
         "perimeter_total,outer_loop_length,n_inner_loops,n_outer_edges\n";
    f << std::fixed << std::setprecision(6);

    // The vector is already sorted score-descending by enumerate_components().
    // Index 0 = highest score = selected.
    for (size_t i = 0; i < comps.size(); ++i) {
        const auto& c = comps[i];
        int is_selected = (i == 0) ? 1 : 0;
        f << c.id << "," << c.centroid_x << "," << c.centroid_y << "," << c.area << ","
          << c.circularity << "," << c.score << "," << is_selected << "," << c.perimeter_total
          << "," << c.outer_loop_length << "," << c.n_inner_loops << "," << c.n_outer_edges << "\n";
    }
    return f.good();
}

// =============================================================================
// Print usage to stdout
// =============================================================================
static void printUsage(const char* argv0) {
    std::cout
        << "Usage:\n"
           "  "
        << argv0
        << " <dicom_dir> <specimen_label> <beta> <slice1> [slice2 ...] [--outdir <DIR>]\n"
           "\n"
           "Arguments:\n"
           "  dicom_dir       Path to DICOM directory (quote if it contains spaces).\n"
           "  specimen_label  Short label used in output filenames (e.g. PHANTOM).\n"
           "  beta            Beta parameter, float (e.g. 0.69).\n"
           "  sliceN          One or more integer slice numbers to process.\n"
           "  --outdir <DIR>  (optional) Override output directory (default: skin_mask_dump/).\n"
           "\n"
           "Example:\n"
           "  "
        << argv0
        << " sample_data/phantom PHANTOM 0.69 1 4 8 --outdir /tmp/out\n"
           "\n"
           "Output files per slice:\n"
           "  <specimen>_slice<NNNN>_ct.raw         — windowed CT uint16 512x512\n"
           "  <specimen>_slice<NNNN>_mask.raw        — skin mask uint8 512x512\n"
           "  <specimen>_slice<NNNN>_comp.raw        — biggest component mask uint8 512x512\n"
           "  <specimen>_slice<NNNN>_components.csv  — per-component score diagnostics\n"
           "  summary.csv                            — per-slice aggregate stats\n";
}

// =============================================================================
// SpecimenSpec — one processing target
// =============================================================================
struct SpecimenSpec {
    std::string dicomDir;
    std::string label;
    double beta;
    std::vector<int> slices;
};

// =============================================================================
// Process all slices for the given target list into outDir.
// Returns {nOk, nFail}.
// =============================================================================
static std::pair<int, int> processTargets(const std::vector<SpecimenSpec>& targets,
                                          const std::string& outDir) {
    fs::create_directories(outDir);
    std::cout << "  output_dir : " << outDir << "\n\n";

    // -------------------------------------------------------------------------
    // Write README alongside the data files
    // -------------------------------------------------------------------------
    {
        std::ofstream readme(outDir + "/README.txt");
        readme
            << "exp_skin_mask_dump output\n"
               "=========================\n"
               "\n"
               "File naming: <specimen>_slice<NNNN>_{ct,mask,comp}.raw\n"
               "             <specimen>_slice<NNNN>_components.csv\n"
               "\n"
               "CT file (*_ct.raw):\n"
               "  dtype  = uint16, little-endian (<u2)\n"
               "  shape  = (512, 512), row-major (C order)\n"
               "  values = windowed CT intensity (WindowCenter/Width applied,\n"
               "           then minimum subtracted), as used by SKINPEELER engine\n"
               "\n"
               "Skin mask file (*_mask.raw):\n"
               "  dtype  = uint8 (u1)\n"
               "  shape  = (512, 512), row-major (C order)\n"
               "  values = 1 (skin pixel), 0 (air / body / background)\n"
               "\n"
               "Component mask file (*_comp.raw):\n"
               "  dtype  = uint8 (u1)\n"
               "  shape  = (512, 512), row-major (C order)\n"
               "  values = 1 (biggest beta-complex component pixel), 0 (background)\n"
               "  note   = cut-independent, pre-peel; this is the Jaccard measurement target.\n"
               "           Extracted via CTImage::extractBiggestComponentMaskFromVoronoi()\n"
               "           (same Voronoi state as calculateShells(), before releaseVoronoi()).\n"
               "           Containment property: skin (mask) is a subset of component (comp).\n"
               "\n"
               "Component CSV (*_components.csv):\n"
               "  Columns: component_id, centroid_x, centroid_y, area, circularity, score, "
               "is_selected\n"
               "  score = area * circularity^2 (same metric as biggest-component selection).\n"
               "  is_selected = 1 for the component actually selected (highest score = index 0).\n"
               "  Sorted by score descending.\n"
               "\n"
               "Skin definition:\n"
               "  m_shells[0]              = S0 = air (outermost) -> NOT skin\n"
               "  m_shells[1..peelIndex]   = skin shells           -> mask=1\n"
               "  m_shells[peelIndex+1..]  = body shells           -> mask=0\n"
               "  peelIndex = m_algoPeelIndex (algorithm-computed cut)\n"
               "\n"
               "Python read example:\n"
               "  import numpy as np\n"
               "  ct   = np.fromfile('PHANTOM_slice0001_ct.raw',   dtype='<u2').reshape(512,512)\n"
               "  mask = np.fromfile('PHANTOM_slice0001_mask.raw',  dtype='u1').reshape(512,512)\n"
               "  comp = np.fromfile('PHANTOM_slice0001_comp.raw',  dtype='u1').reshape(512,512)\n"
               "  # sanity: skin subset of component\n"
               "  assert np.sum((mask==1) & (comp==0)) == 0\n"
               "\n"
               "Generated by: exp_skin_mask_dump (SKINPEELER engine)\n";
    }

    // -------------------------------------------------------------------------
    // Summary CSV
    // -------------------------------------------------------------------------
    std::ofstream summary(outDir + "/summary.csv");
    summary << "specimen,slice_num,beta,skin_pixels,comp_pixels,skin_not_in_comp,peel_index,shell_"
               "count,n_components,status\n";
    summary << std::fixed << std::setprecision(4);

    int nOk = 0, nFail = 0;

    for (const auto& spec : targets) {
        std::cout << "--- Specimen: " << spec.label << "  beta=" << spec.beta
                  << "  dir=" << spec.dicomDir << "\n";

        // Build slice map once per specimen
        auto sliceMap = buildSliceMap(spec.dicomDir);
        std::cout << "  Found " << sliceMap.size() << " .dcm files in directory.\n";

        for (int snum : spec.slices) {
            auto it = sliceMap.find(snum);
            if (it == sliceMap.end()) {
                std::cerr << "  WARNING: slice " << snum << " not found in " << spec.dicomDir
                          << "\n";
                summary << spec.label << "," << snum << "," << spec.beta
                        << ",-1,-1,-1,-1,-1,-1,NOT_FOUND\n";
                ++nFail;
                continue;
            }
            const std::string& dcmPath = it->second;

            std::cout << "  [slice " << std::setw(4) << snum << "] "
                      << fs::path(dcmPath).filename().string() << " ... " << std::flush;

            try {
                CTImage img = CTImage::fromDicomFile(dcmPath, spec.beta);
                img.calculateShells();

                const int H = img.height();
                const int W = img.width();
                const int peelIdx = img.peelIndex();
                const int shellCnt = static_cast<int>(img.shells().size());

                // ---- Component score diagnostics (before releaseVoronoi) ----
                // Must be called while Voronoi is still alive (after calculateShells,
                // before releaseVoronoi).
                auto compScores = img.enumerateComponentScores();
                const int nComps = static_cast<int>(compScores.size());

                // ---- Biggest β-complex component mask (before releaseVoronoi) ----
                std::vector<char> compCharMask = img.extractBiggestComponentMaskFromVoronoi();

                // ---- Biggest-AREA component mask (diagnostic; may differ from
                //      the biggest-SCORE component above). Must run before
                //      releaseVoronoi. Changes internal selected-component state,
                //      so it is done last. ----
                std::vector<char> bigAreaCharMask;
                int bigAreaId = -1;
                if (!compScores.empty()) {
                    int bestIdx = 0;
                    for (int k = 1; k < static_cast<int>(compScores.size()); ++k)
                        if (compScores[k].area > compScores[bestIdx].area)
                            bestIdx = k;
                    bigAreaId = compScores[bestIdx].id;
                    bigAreaCharMask = img.extractComponentMaskById(bigAreaId);
                }

                img.releaseVoronoi();  // free ~340 MB

                // Build output file paths
                std::ostringstream oss;
                oss << outDir << "/" << spec.label << "_slice" << std::setw(4) << std::setfill('0')
                    << snum;
                const std::string basePath = oss.str();
                const std::string ctPath = basePath + "_ct.raw";
                const std::string maskPath = basePath + "_mask.raw";
                const std::string compPath = basePath + "_comp.raw";
                const std::string csvPath = basePath + "_components.csv";

                // ---- CT raw ----
                const std::vector<uint16_t>& ctData = img.image();
                assert(static_cast<int>(ctData.size()) == H * W);
                if (!writeRaw(ctPath, ctData))
                    throw std::runtime_error("cannot write CT raw: " + ctPath);

                // ---- Skin mask ----
                std::vector<uint8_t> mask;
                long long skinPx = buildSkinMask(img, mask);
                if (!writeRaw(maskPath, mask))
                    throw std::runtime_error("cannot write mask raw: " + maskPath);

                // ---- Component mask: convert vector<char> → uint8 ----
                assert(static_cast<int>(compCharMask.size()) == H * W);
                std::vector<uint8_t> compMask(static_cast<size_t>(H * W));
                long long compPx = 0;
                for (int i = 0; i < H * W; ++i) {
                    compMask[static_cast<size_t>(i)] =
                        (compCharMask[static_cast<size_t>(i)] != 0) ? 1u : 0u;
                    if (compMask[static_cast<size_t>(i)])
                        ++compPx;
                }
                if (!writeRaw(compPath, compMask))
                    throw std::runtime_error("cannot write comp raw: " + compPath);

                // ---- Biggest-AREA component mask raw (diagnostic) ----
                if (!bigAreaCharMask.empty()) {
                    std::vector<uint8_t> bigAreaMask(static_cast<size_t>(H * W), 0);
                    for (int i = 0; i < H * W; ++i)
                        bigAreaMask[static_cast<size_t>(i)] =
                            (bigAreaCharMask[static_cast<size_t>(i)] != 0) ? 1u : 0u;
                    if (!writeRaw(basePath + "_bigarea.raw", bigAreaMask))
                        throw std::runtime_error("cannot write bigarea raw");
                }

                // ---- Component score CSV ----
                if (!writeComponentsCsv(csvPath, compScores))
                    throw std::runtime_error("cannot write components CSV: " + csvPath);

                // ---- Sanity: skin ⊆ component ----
                long long skinNotInComp = 0;
                for (int i = 0; i < H * W; ++i) {
                    if (mask[static_cast<size_t>(i)] == 1 && compMask[static_cast<size_t>(i)] == 0)
                        ++skinNotInComp;
                }

                std::cout << "skin_px=" << std::setw(6) << skinPx << "  comp_px=" << std::setw(7)
                          << compPx << "  skin_not_in_comp=" << skinNotInComp
                          << "  peelIdx=" << peelIdx << "  shells=" << shellCnt
                          << "  n_comps=" << nComps;
                if (skinNotInComp > 0)
                    std::cout << "  *** SUBSET VIOLATION ***";
                std::cout << "\n";

                summary << spec.label << "," << snum << "," << spec.beta << "," << skinPx << ","
                        << compPx << "," << skinNotInComp << "," << peelIdx << "," << shellCnt
                        << "," << nComps << ",OK\n";
                ++nOk;

            } catch (const std::exception& e) {
                std::cerr << "FAIL: " << e.what() << "\n";
                summary << spec.label << "," << snum << "," << spec.beta
                        << ",-1,-1,-1,-1,-1,-1,FAIL:" << e.what() << "\n";
                ++nFail;
            }
        }
        std::cout << "\n";
    }

    summary.close();
    return {nOk, nFail};
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[]) {
    std::cout << "=== exp_skin_mask_dump ===\n";

    std::vector<SpecimenSpec> targets;
    std::string outDir = "./skin_mask_dump";

    // CLI arguments are required:
    //   argv[1] = dicom_dir
    //   argv[2] = specimen_label
    //   argv[3] = beta
    //   argv[4..] = slice numbers (integers), optionally followed by
    //               --outdir <DIR> as the last two arguments
    {
        if (argc < 5) {
            std::cerr << "ERROR: insufficient arguments.\n\n";
            printUsage(argv[0]);
            return 1;
        }

        std::string dicomDir = argv[1];
        std::string specimenLabel = argv[2];
        double beta = 0.0;
        try {
            beta = std::stod(argv[3]);
        } catch (...) {
            std::cerr << "ERROR: beta must be a floating-point number, got: " << argv[3] << "\n";
            return 1;
        }

        // Collect slice numbers; detect trailing --outdir <DIR>
        std::vector<int> slices;
        int argEnd = argc;
        if (argc >= 3 && std::string(argv[argc - 2]) == "--outdir") {
            outDir = argv[argc - 1];
            argEnd = argc - 2;
        }

        for (int i = 4; i < argEnd; ++i) {
            try {
                slices.push_back(std::stoi(argv[i]));
            } catch (...) {
                std::cerr << "ERROR: slice number must be integer, got: " << argv[i] << "\n";
                return 1;
            }
        }

        if (slices.empty()) {
            std::cerr << "ERROR: at least one slice number is required.\n\n";
            printUsage(argv[0]);
            return 1;
        }

        targets.push_back({dicomDir, specimenLabel, beta, slices});

        std::cout << "  mode       : CLI\n";
        std::cout << "  dicom_dir  : " << dicomDir << "\n";
        std::cout << "  specimen   : " << specimenLabel << "\n";
        std::cout << "  beta       : " << beta << "\n";
        std::cout << "  slices     :";
        for (int s : slices)
            std::cout << " " << s;
        std::cout << "\n";
    }

    auto [nOk, nFail] = processTargets(targets, outDir);

    std::cout << "=== Summary ===\n";
    std::cout << "  OK   : " << nOk << "\n";
    std::cout << "  FAIL : " << nFail << "\n";
    std::cout << "  summary CSV: " << outDir << "/summary.csv\n";
    std::cout << "  README     : " << outDir << "/README.txt\n";

    return (nFail > 0) ? 2 : 0;
}

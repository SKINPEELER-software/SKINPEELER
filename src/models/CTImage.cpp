#include "CTImage.h"

// DCMTK
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmimgle/dipixel.h>

// DCMTK JPEG Lossless codec (Transfer Syntax 1.2.840.10008.1.2.4.70)
// Must be registered once before any JPEG-compressed DICOM is decoded.
#include <dcmtk/dcmjpeg/djdecode.h>

// V library (ImageProcessingBC)
#include "ImageProcessingBC.h"
#include "VoroConst.h"

// BedDetector + cache
#include "BedCache.h"

// Pure cutting-statistics math (S1 extraction — no CTImage/V-IP/DCMTK dependency)
#include "ShellStatistics.h"
#include "BetaSelection.h"

#include "utils/CoordinateTransform.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <cassert>
#include <cstdio>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// Shell traversal on the quasi-triangulation
//
// One traversal step yields the vertices of one shell plus the "front" (vertex
// handles) that seeds the next step. Two interchangeable implementations with
// identical vertex order and visited-flag semantics:
//
//   SKINPEELER_HAVE_SHELL_FRONT == 1  ImageProcessingBC shell-front primitives
//       (collect_outer_shell_front / expand_shell_front): work on the raw
//       Voronoi vertex pointers and allocate no wrapper objects.
//   SKINPEELER_HAVE_SHELL_FRONT == 0  QuasiVertex / QuasiEdge wrapper loop, for
//       a V-ImageProcessing library build that does not export the shell-front
//       primitives.
//
// CMake sets the macro from the linked V-ImageProcessing library (see the
// V-ImageProcessing section of the top-level CMakeLists.txt).
// =============================================================================
#ifndef SKINPEELER_HAVE_SHELL_FRONT
#define SKINPEELER_HAVE_SHELL_FRONT 1
#endif

#if !SKINPEELER_HAVE_SHELL_FRONT
#include "QuasiVertex.h"
#include "QuasiEdge.h"
#endif

namespace {

#if SKINPEELER_HAVE_SHELL_FRONT

using ShellFront = V::ImageProcessingBC::ShellFront;

// Build a CTShell from one traversal step's vertices.
CTShell makeShell(const std::vector<V::ImageProcessingBC::ShellVertex>& verts) {
    CTShell shell;
    for (const auto& v : verts)
        shell.addVertex(v.x, v.y, v.radius);
    return shell;
}

// Shell[0] of the biggest component (selection score): its outer-loop
// vertices. `front` receives the vertex handles.
CTShell collectOuterShell(V::ImageProcessingBC& bc, ShellFront& front) {
    bc.select_biggest_componentBSFace();
    std::vector<V::ImageProcessingBC::ShellVertex> verts;
    bc.collect_outer_shell_front(front, verts);
    return makeShell(verts);
}

// Selects component `componentId` as the current component. Returns true and
// fills `shell` / `front` with its outer-loop vertices when it has a non-empty
// outer loop; returns false (outputs untouched) otherwise.
bool collectOuterShellOfComponent(V::ImageProcessingBC& bc, int componentId, ShellFront& front,
                                  CTShell& shell) {
    if (!bc.select_componentBSFace_by_id(componentId) ||
        bc.get_n_outer_edges_of_biggest_component() <= 0)
        return false;
    std::vector<V::ImageProcessingBC::ShellVertex> verts;
    bc.collect_outer_shell_front(front, verts);
    shell = makeShell(verts);
    return true;
}

// Next shell inward from `prevFront`: not-yet-visited end points of the
// REGULAR / INTERIOR beta-edges incident to the previous shell's vertices.
CTShell expandShell(V::ImageProcessingBC& bc, double beta, const ShellFront& prevFront,
                    ShellFront& newFront) {
    std::vector<V::ImageProcessingBC::ShellVertex> verts;
    bc.expand_shell_front(beta, prevFront, newFront, verts);
    return makeShell(verts);
}

#else  // !SKINPEELER_HAVE_SHELL_FRONT

using ShellFront = std::vector<V::QuasiVertex>;

// Adds the not-yet-visited end points of `edge` to `shell` / `front` and marks
// them visited.
template <typename Edge>
void addUnvisitedEndPoints(Edge& edge, CTShell& shell, ShellFront& front) {
    auto sv = edge.get_start_quasi_vertex();
    auto ev = edge.get_end_quasi_vertex();

    if (!sv.is_visited()) {
        sv.set_visited(true);
        auto pt = sv.get_point();
        shell.addVertex(pt.getX(), pt.getY(), sv.get_radius());
        front.push_back(std::move(sv));
    }
    if (!ev.is_visited()) {
        ev.set_visited(true);
        auto pt = ev.get_point();
        shell.addVertex(pt.getX(), pt.getY(), ev.get_radius());
        front.push_back(std::move(ev));
    }
}

// Shell[0] of the biggest component (selection score): its outer-loop
// vertices. `front` receives the vertex handles.
CTShell collectOuterShell(V::ImageProcessingBC& bc, ShellFront& front) {
    auto outerEdges = bc.get_edge_of_outer_loop_of_biggest_componentBSFace();
    CTShell shell;
    front.clear();
    for (auto& edge : outerEdges)
        addUnvisitedEndPoints(edge, shell, front);
    return shell;
}

// Selects component `componentId` as the current component. Returns true and
// fills `shell` / `front` with its outer-loop vertices when it has a non-empty
// outer loop; returns false (outputs untouched) otherwise.
bool collectOuterShellOfComponent(V::ImageProcessingBC& bc, int componentId, ShellFront& front,
                                  CTShell& shell) {
    auto outerEdges = bc.get_edge_of_outer_loop_of_componentBSFace(componentId);
    if (outerEdges.empty())
        return false;
    CTShell newShell;
    front.clear();
    for (auto& edge : outerEdges)
        addUnvisitedEndPoints(edge, newShell, front);
    shell = std::move(newShell);
    return true;
}

// Next shell inward from `prevFront`: not-yet-visited end points of the
// REGULAR / INTERIOR beta-edges incident to the previous shell's vertices.
CTShell expandShell(V::ImageProcessingBC& /*bc*/, double beta, const ShellFront& prevFront,
                    ShellFront& newFront) {
    CTShell newShell;
    newFront.clear();
    for (const auto& vert : prevFront) {
        V::QuasiVertex v = vert;
        auto incEdges = v.find_incident_quasi_edges();

        for (auto& edge : incEdges) {
            int state = edge.get_bounding_state(beta);
            if (state == V::REGULAR_STATE || state == V::INTERIOR_STATE)
                addUnvisitedEndPoints(edge, newShell, newFront);
        }
    }
    return newShell;
}

#endif  // SKINPEELER_HAVE_SHELL_FRONT

}  // namespace

// =============================================================================
// Internal Voronoi wrapper  (keeps V headers out of CTImage.h)
// Holds the outer-shell front so that calculateInnerShellsAdaptive() can use
// it as the seed of the inward traversal.
// =============================================================================
struct CTImage::VoronoiImpl {
    V::ImageProcessingBC bc;
    ShellFront outerShellFront;  // populated by extractOuterShell()
    // True once a shell traversal has marked vertices visited on this
    // structure: the next evaluation (another beta in sweepBetas()) must clear
    // the vertex visited flags first.
    bool traversed = false;
};

// =============================================================================
// Factory
// =============================================================================
CTImage CTImage::fromDicomFile(const std::string& filepath, double beta, int defaultThickness) {
    // Register JPEG decoders once (idempotent — DCMTK ignores repeated calls
    // when codecs are already registered). This enables Transfer Syntax
    // 1.2.840.10008.1.2.4.70 (JPEG Lossless) and all other JPEG processes.
    // Thread-safe: DCMTK uses an internal mutex for codec registration.
    static std::once_flag s_jpegCodecFlag;
    std::call_once(s_jpegCodecFlag, []() { DJDecoderRegistration::registerCodecs(); });

    auto ff = std::make_unique<DcmFileFormat>();
    OFCondition status = ff->loadFile(filepath.c_str());
    if (status.bad())
        throw DicomLoadError(std::string(ErrorMessages::FILE_READ_ERROR) + ": " + status.text());

    DcmDataset* ds = ff->getDataset();
    if (!ds)
        throw DicomLoadError(ErrorMessages::INVALID_DICOM);

    // Confirm TransferSyntaxUID exists
    DcmMetaInfo* meta = ff->getMetaInfo();
    if (!meta || !meta->tagExists(DCM_TransferSyntaxUID))
        throw DicomLoadError(ErrorMessages::MISSING_TRANSFER_SYNTAX);

    return CTImage(std::move(ff), filepath, beta, defaultThickness);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
CTImage::CTImage(std::unique_ptr<DcmFileFormat> fileFormat, const std::string& filepath,
                 double beta, int defaultThickness)
    : m_fileFormat(std::move(fileFormat)),
      m_filepath(filepath),
      m_beta(beta),
      m_defaultThickness(defaultThickness) {
    extractImageFromDicom();
}

CTImage::~CTImage() = default;
CTImage::CTImage(CTImage&&) noexcept = default;
CTImage& CTImage::operator=(CTImage&&) noexcept = default;

// =============================================================================
// DICOM Image Extraction
// Matches Python: slope*pixel_array + intercept → apply_voi_lut → subtract min
// Window Center / Window Width are read explicitly from the DICOM tags and
// passed to DicomImage::setWindow() so the correct preset is used. Without
// this, DCMTK may default to the full HU range (-1024..3071), making the
// image appear very low-contrast (washed out / grey).
// =============================================================================
void CTImage::extractImageFromDicom() {
    DcmDataset* ds = m_fileFormat->getDataset();

    // 1. Dimensions
    Uint16 rows = 0, cols = 0;
    ds->findAndGetUint16(DCM_Rows, rows);
    ds->findAndGetUint16(DCM_Columns, cols);
    m_height = static_cast<int>(rows);
    m_width = static_cast<int>(cols);
    const int nPixels = m_height * m_width;

    // 3. Use DicomImage for automatic decompression + Rescale slope/intercept.
    //    DicomImage handles JPEG Lossless (TS 1.2.840.10008.1.2.4.70) once
    //    DJDecoderRegistration::registerCodecs() has been called (done above in
    //    fromDicomFile()). Must be created before attempting pixel extraction.
    DicomImage dicomImg(m_filepath.c_str());
    if (dicomImg.getStatus() != EIS_Normal)
        throw DicomLoadError("DicomImage processing failed");

    // 4. Explicitly read Window Center / Window Width from DICOM tags.
    //    DCMTK may not pick the correct preset automatically — this matches
    //    pydicom apply_voi_lut() which uses the first WC/WW entry by default.
    {
        DcmElement* wcElem = nullptr;
        DcmElement* wwElem = nullptr;
        if (ds->findAndGetElement(DCM_WindowCenter, wcElem).good() && wcElem &&
            ds->findAndGetElement(DCM_WindowWidth, wwElem).good() && wwElem) {
            Float64 wc = 0.0, ww = 0.0;
            // Use the FIRST value (index 0) — matches pydicom default behaviour
            if (wcElem->getFloat64(wc, 0).good() && wwElem->getFloat64(ww, 0).good() && ww > 0.0) {
                dicomImg.setWindow(wc, ww);
            }
        }
        // If no window tags: DCMTK uses auto-window from image statistics (best fallback)
    }

    // 5. Get 16-bit output with the window applied
    const uint16_t* outPtr = static_cast<const uint16_t*>(dicomImg.getOutputData(16));
    if (!outPtr)
        throw DicomLoadError("Failed to get DicomImage output data");

    m_image.assign(outPtr, outPtr + nPixels);

    // 2. Raw DICOM pixel array for export after peeling.
    //    For uncompressed DICOM: read directly from dataset.
    //    For JPEG-compressed DICOM: pixel data is encapsulated; use the
    //    decompressed DicomImage output as the "raw" baseline. This means
    //    re-saved files after peeling will be uncompressed (lossless for
    //    the algorithm's purposes — peeling zeroes out skin pixels anyway).
    {
        const Uint16* rawPtr = nullptr;
        unsigned long rawCount = 0;
        ds->findAndGetUint16Array(DCM_PixelData, rawPtr, &rawCount);
        if (rawPtr && static_cast<int>(rawCount) >= nPixels) {
            // Uncompressed: use dataset pixel array directly
            m_originalImage.assign(rawPtr, rawPtr + nPixels);
        } else {
            // Compressed (e.g. JPEG Lossless): use decompressed display data.
            // DicomImage.getOutputData(16) returns 16-bit windowed values;
            // we need the unscaled 16-bit pixels. Use getInterData() to get
            // the internally decoded (rescale-applied) 16-bit array.
            const DiPixel* diPixel = dicomImg.getInterData();
            if (diPixel && diPixel->getData()) {
                const Uint16* rawData = static_cast<const Uint16*>(diPixel->getData());
                m_originalImage.assign(rawData, rawData + nPixels);
            } else {
                // Final fallback: use the windowed output (adequate for display;
                // peeled DICOM export may differ from original HU values)
                m_originalImage.assign(outPtr, outPtr + nPixels);
            }
        }
        // Determine m_minIntensity = the raw uint16 bit pattern that represents
        // the minimum (air/background) pixel in this slice.
        //
        // For PixelRepresentation=1 (signed int16) datasets — common in CT —
        // the uint16 min is NOT the background air pixel.  Example: GC20-1006
        //   air pixel = int16(-2048) = uint16(63488)  [0xF800]
        //   uint16 min = 0  (water/soft-tissue region)
        // Using uint16 min (0) as the zeroing target produces HU=0 (grey),
        // not HU=-2048 (air/black).
        //
        // Fix: when PixelRepresentation==1, reinterpret the stored uint16 array
        // as int16, find the int16 minimum, then reinterpret that value back to
        // uint16 for consistent storage in m_minIntensity.
        if (!m_originalImage.empty()) {
            Uint16 pixelRep = 0;
            ds->findAndGetUint16(DCM_PixelRepresentation, pixelRep);
            if (pixelRep == 1) {
                // Signed int16 dataset: find min in signed domain
                const int16_t* sPtr = reinterpret_cast<const int16_t*>(m_originalImage.data());
                const int16_t* sEnd = sPtr + m_originalImage.size();
                int16_t sMin = *std::min_element(sPtr, sEnd);
                // Store as the uint16 bit pattern (memcpy is well-defined)
                uint16_t uMin = 0;
                static_assert(sizeof(int16_t) == sizeof(uint16_t), "size mismatch");
                std::memcpy(&uMin, &sMin, sizeof(uint16_t));
                m_minIntensity = uMin;
            } else {
                // Unsigned: straightforward
                m_minIntensity = *std::min_element(m_originalImage.begin(), m_originalImage.end());
            }
        } else {
            m_minIntensity = 0;
        }
    }

    // 6. Subtract minimum — matches Python: image -= np.min(image)
    uint16_t minVal = *std::min_element(m_image.begin(), m_image.end());
    for (auto& p : m_image)
        p -= minVal;

    m_imageMax = static_cast<double>(*std::max_element(m_image.begin(), m_image.end()));

    // 7. Read Slice Location for sorting (fallback: InstanceNumber, then 0)
    {
        Float64 sl = 0.0;
        if (ds->findAndGetFloat64(DCM_SliceLocation, sl).good()) {
            m_sliceLocation = sl;
        } else {
            Sint32 instNum = 0;
            if (ds->findAndGetSint32(DCM_InstanceNumber, instNum).good())
                m_sliceLocation = static_cast<double>(instNum);
        }
    }
}

// =============================================================================
// normalizeToMax  — cross-slice intensity normalization
// Python: self._image = (65535 * self._image / global_max).astype(np.uint16)
// =============================================================================
void CTImage::normalizeToMax(double globalMax) {
    if (globalMax <= 0.0)
        globalMax = 1.0;
    for (auto& p : m_image)
        p = static_cast<uint16_t>(IMAGE_MAX_VALUE * static_cast<double>(p) / globalMax);
}

void CTImage::setBeta(double b) {
    m_beta = std::max(0.0, std::min(1.0, b));
    m_voronoi.reset();

    // Everything derived from the beta-complex goes stale the moment beta
    // changes and must be recomputed by the next calculateShells() call.
    // Leaving it behind previously let callers read peel/cut/bed state
    // computed under the OLD beta.
    clearDerivedState();
}

void CTImage::clearDerivedState() {
    m_shells.clear();
    m_peelIndex = 0;
    m_algoPeelIndex = 0;
    m_peeled = false;
    m_shellMahalD.clear();
    m_shellInside.clear();
    m_mahalFit = MahalFit{};
    m_bedResult = BedDetector::Result{};
    m_cutRule = CutRule::Unknown;
    m_componentSummary = ComponentSummary{};
}

// =============================================================================
// calculateShells  — main entry point for Voronoi + shell extraction
// Always extracts all reachable shells (maximize mode). See CTImage.h for rationale.
// =============================================================================
void CTImage::calculateShells() {
    m_timedOut = false;
    try {
        buildVoronoi();
        evaluateCurrentBeta();
    } catch (const std::exception& e) {
        throw VoronoiError(std::string(ErrorMessages::SHELL_CALC_ERROR) + ": " + e.what());
    }
}

// =============================================================================
// buildVoronoi — beta-independent Voronoi / quasi-triangulation construction
// (~1.7 s, ~340 MB). Replaces any existing structure.
// =============================================================================
void CTImage::buildVoronoi() {
    m_voronoi = std::make_unique<VoronoiImpl>();
    V::ImageProcessingBC& bc = m_voronoi->bc;

    // Transpose image to column-major for the C++ library
    std::vector<std::vector<int>> colorDataSet(m_width, std::vector<int>(m_height));
    for (int r = 0; r < m_height; ++r)
        for (int c = 0; c < m_width; ++c)
            colorDataSet[c][r] = static_cast<int>(m_image[r * m_width + c]);

    // Read BitsStored from DICOM — required for correct 16-bit → 8-bit scaling
    // Python V1 ct.py: construct_for_Image_Processing_16bit(colorDataSet, bits_stored, ...)
    Uint16 bitsStored = 16;
    if (m_fileFormat && m_fileFormat->getDataset())
        m_fileFormat->getDataset()->findAndGetUint16(DCM_BitsStored, bitsStored);
    const int bitsStoredInt = static_cast<int>(bitsStored);

    bc.constructForImageProcessing16bit(colorDataSet, bitsStoredInt, DARKER_PIXEL_BIGGER_DISK);
}

// =============================================================================
// evaluateCurrentBeta — everything that depends on beta, on the Voronoi
// structure built by buildVoronoi(): component classification/selection,
// optional bed removal, shell extraction, cutting rule.
// Shared by calculateShells() (one beta) and sweepBetas() (many betas).
// May throw; callers translate/handle.
// =============================================================================
void CTImage::evaluateCurrentBeta() {
    assert(m_voronoi);
    V::ImageProcessingBC& bc = m_voronoi->bc;

    bc.set_beta_value(m_beta);
    bc.classifyByComponents();

    // A previous evaluation on this structure left vertex visited flags
    // set (classifyByComponents() resets face flags only).
    if (m_voronoi->traversed)
        bc.reset_vertex_visited();
    m_voronoi->traversed = true;

    m_shells.clear();
    extractOuterShell();

    // =====================================================================
    // Bed removal hook (inserted after extractOuterShell, before inner shells)
    // Only active when m_bedRemovalEnabled == true.
    // When disabled: this block is never entered → existing code path 100% unchanged.
    // =====================================================================
    if (m_bedRemovalEnabled) {
        // --- Cache lookup ---
        std::string cacheDir = m_specimenDir.empty()
                                   ? std::filesystem::path(m_filepath).parent_path().string()
                                   : m_specimenDir;

        bool cacheHit = false;
        if (BedCache::exists(cacheDir, m_beta, BedCache::ALGORITHM_VERSION)) {
            cacheHit = BedCache::load(cacheDir, m_beta, BedCache::ALGORITHM_VERSION, m_filepath,
                                      m_bedResult);
        }

        // enumerate_components() is called exactly once per calculateShells()
        // invocation (score-descending; index 0 = current body0), regardless
        // of cache hit/miss. Needed below both for BedDetector input (on a
        // cache miss) and for current_body0_id (always, to detect a change).
        auto vd_comps_raw = bc.enumerate_components();
        int current_body0_id = vd_comps_raw.empty() ? -1 : vd_comps_raw[0].id;

        if (!cacheHit) {
            // --- No cache: run BedDetector ---
            // Convert ImageProcessingBC::ComponentInfo → BedDetector::VDComponentInfo
            std::vector<BedDetector::VDComponentInfo> vd_comps;
            vd_comps.reserve(vd_comps_raw.size());
            for (const auto& ci : vd_comps_raw) {
                BedDetector::VDComponentInfo vi;
                vi.id = ci.id;
                vi.centroid_x = ci.centroid_x;
                vi.centroid_y = ci.centroid_y;
                vi.area = ci.area;
                vi.circularity = ci.circularity;
                vi.score = ci.score;
                vd_comps.push_back(vi);
            }

            // body0_cy: mean y of shell[0] vertices (Voronoi space)
            double body0_cy = 0.0;
            if (!m_shells.empty() && !m_shells[0].isEmpty()) {
                for (const auto& v : m_shells[0])
                    body0_cy += v.y;
                body0_cy /= static_cast<double>(m_shells[0].size());
            }

            BedDetector bd;
            m_bedResult = bd.detect(vd_comps, m_image.data(), m_width, m_height, body0_cy,
                                    BedDetector::Params{});

            // Cache save (single slice upsert)
            BedCache::save(cacheDir, m_beta, BedCache::ALGORITHM_VERSION, m_filepath, m_bedResult);
        }

        // --- body0 replacement ---
        const int new_id = m_bedResult.new_body0_component_id;

        if (new_id != -1 && new_id != current_body0_id) {
            // Selects the component (also when its outer loop turns out to
            // be empty); shell[0] and the outer front are rebuilt only for a
            // non-empty outer loop.
            CTShell newShell;
            if (collectOuterShellOfComponent(bc, new_id, m_voronoi->outerShellFront, newShell)) {
                m_shells.clear();
                newShell.calculateStatistics();
                m_shells.push_back(std::move(newShell));
            }
            // m_image not modified, VD not rebuilt
        }
    }
    // =====================================================================
    // End bed removal hook
    // =====================================================================

    recordComponentSummary();

    calculateInnerShellsAdaptive(true);
    m_algoPeelIndex = calculateCuttingShell();
    m_peelIndex = m_algoPeelIndex;
}

void CTImage::recordComponentSummary() {
    V::ImageProcessingBC& bc = m_voronoi->bc;
    m_componentSummary.valid = true;
    m_componentSummary.nInnerLoops = bc.get_n_inner_loops_of_biggest_component();
    m_componentSummary.nComponents = bc.get_n_components();
    m_componentSummary.area = bc.get_area_of_biggest_component();
    m_componentSummary.outerCircularity = bc.get_outer_circularity_of_biggest_component();
}

// =============================================================================
// sweepBetas — one Voronoi structure, many betas (see CTImage.h)
// =============================================================================
void CTImage::sweepBetas(const std::vector<double>& betas, const BetaCallback& onBeta) {
    if (betas.empty())
        return;

    // Release the Voronoi structure on every exit path (including a throwing
    // callback).
    struct VoronoiReleaser {
        CTImage& self;
        ~VoronoiReleaser() {
            self.m_voronoi.reset();
        }
    } releaser{*this};

    try {
        buildVoronoi();
    } catch (const std::exception& e) {
        throw VoronoiError(std::string(ErrorMessages::SHELL_CALC_ERROR) + ": " + e.what());
    }

    for (double b : betas) {
        m_beta = std::max(0.0, std::min(1.0, b));  // same clamp as setBeta()
        clearDerivedState();
        m_timedOut = false;

        BetaEvaluation ev;
        try {
            evaluateCurrentBeta();
            ev = summarizeEvaluation();
            ev.ok = true;
        } catch (const std::exception& e) {
            clearDerivedState();
            ev = BetaEvaluation{};
            ev.beta = m_beta;
            ev.ok = false;
            ev.error = std::string(ErrorMessages::SHELL_CALC_ERROR) + ": " + e.what();
        }
        if (onBeta)
            onBeta(ev);
    }
}

CTImage::BetaEvaluation CTImage::summarizeEvaluation() const {
    BetaEvaluation ev;
    ev.beta = m_beta;
    ev.ok = true;
    ev.component = m_componentSummary;
    ev.cut = m_algoPeelIndex;
    ev.cutRule = m_cutRule;
    ev.fit = m_mahalFit;
    ev.timedOut = m_timedOut;
    ev.judgeable = judgeable();
    ev.b2Contrast = b2Contrast();

    const int n = static_cast<int>(m_shells.size());
    ev.shells.resize(n);
    for (int i = 0; i < n; ++i) {
        ShellSummary& sh = ev.shells[i];
        sh.mu = m_shells[i].mu();
        sh.sigma = m_shells[i].sigma();
        sh.nVertices = static_cast<int>(m_shells[i].size());
        sh.hasStatistics = m_shells[i].hasStatistics();
        // Convention of the cutting rules: -1 = shell not scored.
        if (i < static_cast<int>(m_shellMahalD.size()) && m_shellMahalD[i] >= 0.0) {
            sh.hasMahal = true;
            sh.mahalD = m_shellMahalD[i];
            sh.mahalD2 = sh.mahalD * sh.mahalD;  // same squaring the BIC changepoint uses
        }
        sh.inside = (i < static_cast<int>(m_shellInside.size())) ? m_shellInside[i] : false;
    }
    return ev;
}

double CTImage::b2Contrast() const {
    if (!m_mahalFit.active)
        return std::numeric_limits<double>::quiet_NaN();
    // The fit is on S2..Sm: m_shells index (offset + 1) onward, where offset
    // is the index of S1.
    const int offset = EXCLUDE_OUTERMOST_SHELL ? 1 : 0;
    return BetaSelection::b2SeparationContrast(m_shellMahalD, m_algoPeelIndex, offset + 1);
}

const char* CTImage::cutRuleName(CutRule rule) {
    switch (rule) {
        case CutRule::Unknown:
            return "UNKNOWN";
        case CutRule::None:
            return "NONE";
        case CutRule::MahalBic:
            return "MAHAL_BIC";
        case CutRule::ThreeSigma:
            return "THREE_SIGMA";
        case CutRule::Euclidean:
            return "EUCLIDEAN";
        case CutRule::EuclideanFallback:
            return "EUCLIDEAN_FALLBACK";
    }
    return "UNKNOWN";
}

const char* CTImage::cuttingAlgorithmName(CuttingAlgorithm algo) {
    switch (algo) {
        case CuttingAlgorithm::THREE_SIGMA:
            return "THREE_SIGMA";
        case CuttingAlgorithm::GMM:
            return "GMM";
        case CuttingAlgorithm::EUCLIDEAN:
            return "EUCLIDEAN";
        case CuttingAlgorithm::MAHAL_BIC:
            return "MAHAL_BIC";
    }
    return "UNKNOWN";
}

bool CTImage::selectionUsesOuterLoopCircularity() {
    return V::ImageProcessingBC::get_use_outer_loop_circularity();
}

// =============================================================================
// shellMask / skinMask — rasterize shell vertices to a pixel mask
// =============================================================================
std::vector<uint8_t> CTImage::shellMask(int firstShell, int lastShell) const {
    std::vector<uint8_t> mask(static_cast<size_t>(m_height) * static_cast<size_t>(m_width), 0);
    const int nShells = static_cast<int>(m_shells.size());
    const int lo = std::max(0, firstShell);
    const int hi = std::min(lastShell, nShells - 1);
    for (int si = lo; si <= hi; ++si) {
        for (const auto& v : m_shells[si]) {
            auto [row, col] = CoordinateTransform::voronoiToImagePixel(v.x, v.y, m_height, m_width);
            if (CoordinateTransform::isValidPixel(row, col, m_height, m_width))
                mask[static_cast<size_t>(row) * m_width + col] = 1;
        }
    }
    return mask;
}

std::vector<uint8_t> CTImage::skinMask(int cut) const {
    return shellMask(0, cut >= 0 ? cut : m_peelIndex);
}

// =============================================================================
// noAirMasks — air-excluded skin mask and air mask (definition in CTImage.h)
// =============================================================================
CTImage::NoAirMasks CTImage::noAirMasks(int cut) const {
    NoAirMasks out;
    const size_t nPixels = static_cast<size_t>(m_height) * static_cast<size_t>(m_width);

    const std::vector<uint8_t> s0 = shellMask(0, 0);
    const std::vector<uint8_t> cutMsk = skinMask(cut);

    // T = max intensity over S0 pixels; -infinity when S0 is empty.
    double threshold = -std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < nPixels; ++i) {
        if (s0[i]) {
            ++out.nS0Pixels;
            threshold = std::max(threshold, static_cast<double>(m_image[i]));
        }
    }
    out.s0Empty = (out.nS0Pixels == 0);
    out.airThreshold = threshold;

    out.skin.assign(nPixels, 0);
    out.air.assign(nPixels, 0);
    for (size_t i = 0; i < nPixels; ++i) {
        const bool keep = static_cast<double>(m_image[i]) > threshold;
        if (cutMsk[i] && !s0[i] && keep) {
            out.skin[i] = 1;
            ++out.nSkinPixels;
        }
        if (s0[i] || !keep) {
            out.air[i] = 1;
            ++out.nAirPixels;
        }
    }
    return out;
}

// =============================================================================
// extractOuterShell
// Selects the biggest component (selection score) and collects its outer-loop
// vertices as shell[0]. The vertex handles are kept in
// m_voronoi->outerShellFront so that calculateInnerShellsAdaptive() can use
// them as seeds.
// =============================================================================
void CTImage::extractOuterShell() {
    assert(m_voronoi);

    CTShell shell = collectOuterShell(m_voronoi->bc, m_voronoi->outerShellFront);
    shell.calculateStatistics();
    m_shells.push_back(std::move(shell));
}

// =============================================================================
// calculateInnerShellsAdaptive — inward shell traversal from the outer front
// =============================================================================
void CTImage::calculateInnerShellsAdaptive(bool maximize) {
    if (!m_voronoi)
        return;
    V::ImageProcessingBC& bc = m_voronoi->bc;

    // Time limit safety: record start time
    const auto calcStartTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = [&calcStartTime]() -> double {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - calcStartTime).count();
    };

    // Use the outer-shell front cached by extractOuterShell().
    // Ping-pong: only keep the previous shell's front as seeds for the next.
    ShellFront prevFront = m_voronoi->outerShellFront;

    // Incremental shell statistics for adaptive stopping (avoid repeated full traversal)
    std::vector<std::pair<double, double>> statsPoints;
    if (EXCLUDE_OUTERMOST_SHELL && !m_shells.empty() && m_shells[0].hasStatistics()) {
        // shell[0] excluded from stats — start empty
    }
    for (int si = (EXCLUDE_OUTERMOST_SHELL ? 1 : 0); si < static_cast<int>(m_shells.size()); ++si) {
        const CTShell& sh = m_shells[si];
        if (sh.hasStatistics())
            statsPoints.emplace_back(sh.mu(), sh.sigma());
    }

    // Shared lambda: expand one shell from prevFront
    auto expandOneShell = [&](ShellFront& newFront) -> CTShell {
        return expandShell(bc, m_beta, prevFront, newFront);
    };

    // Calculate minimum shells first
    for (int i = static_cast<int>(m_shells.size()); i < ADAPTIVE_SHELL_MIN; ++i) {
        if (prevFront.empty())
            break;
        if (elapsedSeconds() > ADAPTIVE_TIME_LIMIT_SECONDS) {
            m_timedOut = true;
            break;
        }

        ShellFront newFront;
        CTShell newShell = expandOneShell(newFront);

        if (!newShell.isEmpty()) {
            newShell.calculateStatistics();
            if (newShell.hasStatistics())
                statsPoints.emplace_back(newShell.mu(), newShell.sigma());
            m_shells.push_back(std::move(newShell));
            prevFront = std::move(newFront);
        } else {
            break;
        }
    }

    // Adaptive stopping: check every ADAPTIVE_CHECK_INTERVAL shells
    int consecutiveInside = 0;

    for (int shellIdx = static_cast<int>(m_shells.size()); shellIdx < ADAPTIVE_SHELL_MAX;
         ++shellIdx) {
        if (prevFront.empty())
            break;

        ShellFront newFront;
        CTShell newShell = expandOneShell(newFront);

        if (newShell.isEmpty())
            break;

        // Time limit check (primary safety mechanism)
        if (elapsedSeconds() > ADAPTIVE_TIME_LIMIT_SECONDS) {
            m_timedOut = true;
            break;
        }

        newShell.calculateStatistics();
        if (newShell.hasStatistics())
            statsPoints.emplace_back(newShell.mu(), newShell.sigma());
        m_shells.push_back(std::move(newShell));
        prevFront = std::move(newFront);

        // Check adaptive stopping every N shells (using incremental statsPoints)
        if (shellIdx % ADAPTIVE_CHECK_INTERVAL == 0 && statsPoints.size() >= 4) {
            const double np = static_cast<double>(statsPoints.size());
            double meanMu = 0.0, meanSigma = 0.0;
            for (auto& [mu, sigma] : statsPoints) {
                meanMu += mu;
                meanSigma += sigma;
            }
            meanMu /= np;
            meanSigma /= np;

            double c00 = 0, c01 = 0, c11 = 0;
            for (auto& [mu, sigma] : statsPoints) {
                double dm = mu - meanMu;
                double ds = sigma - meanSigma;
                c00 += dm * dm;
                c01 += dm * ds;
                c11 += ds * ds;
            }
            const double div = np - 1.0;
            double cov[2][2] = {{c00 / div, c01 / div}, {c01 / div, c11 / div}};
            double inv[2][2];

            if (ShellStatistics::invert2x2(cov, inv)) {
                const auto& [lmu, lsig] = statsPoints.back();
                double diff[2] = {lmu - meanMu, lsig - meanSigma};
                double mahal = ShellStatistics::mahalanobis2D(diff, inv);

                if (!maximize) {
                    if (mahal <= SIGMA_THRESHOLD) {
                        ++consecutiveInside;
                        if (consecutiveInside >= ADAPTIVE_STOPPING_CONSECUTIVE)
                            break;
                    } else {
                        consecutiveInside = 0;
                    }
                }
            }
        }
    }
}

void CTImage::releaseVoronoi() {
    m_voronoi.reset();
}

// =============================================================================
// extractBiggestComponentMaskFromVoronoi — reuse existing Voronoi state
//
// Must be called after calculateShells() and before releaseVoronoi().
// Calls select_biggest_componentBSFace() again to ensure state is correct,
// then extracts the pixel mask via biggest_component_pixel_mask().
// =============================================================================
std::vector<char> CTImage::extractBiggestComponentMaskFromVoronoi() {
    if (!m_voronoi) {
        throw VoronoiError(
            "extractBiggestComponentMaskFromVoronoi: Voronoi not available. "
            "Call calculateShells() before releaseVoronoi().");
    }

    V::ImageProcessingBC& bc = m_voronoi->bc;

    try {
        // select_biggest_componentBSFace() is idempotent; calling it again is safe
        // and ensures the state is correct even if calculateShells() used a
        // different biggest-component selection path.
        bc.select_biggest_componentBSFace();

        std::vector<char> mask =
            bc.biggest_component_pixel_mask(m_height, m_width, COORDINATE_OFFSET);

        return mask;

    } catch (const std::exception& e) {
        throw VoronoiError(std::string("extractBiggestComponentMaskFromVoronoi failed: ") +
                           e.what());
    }
}

// =============================================================================
// enumerateComponentScores — expose all β-complex component scores
//
// Must be called after calculateShells() and before releaseVoronoi().
// Returns score-descending sorted vector (mirrors ImageProcessingBC::enumerate_components).
// =============================================================================
std::vector<CTImage::ComponentScoreInfo> CTImage::enumerateComponentScores() {
    if (!m_voronoi) {
        throw VoronoiError(
            "enumerateComponentScores: Voronoi not available. "
            "Call calculateShells() before releaseVoronoi().");
    }

    V::ImageProcessingBC& bc = m_voronoi->bc;
    auto raw = bc.enumerate_components_with_geometry();

    std::vector<ComponentScoreInfo> result;
    result.reserve(raw.size());
    for (const auto& ci : raw) {
        ComponentScoreInfo info;
        info.id = ci.id;
        info.centroid_x = ci.centroid_x;
        info.centroid_y = ci.centroid_y;
        info.area = ci.area;
        info.circularity = ci.circularity;
        info.score = ci.score;
        info.perimeter_total = ci.perimeter_total;
        info.outer_loop_length = ci.outer_loop_length;
        info.n_inner_loops = ci.n_inner_loops;
        info.n_outer_edges = ci.n_outer_edges;
        result.push_back(info);
    }
    return result;
}

// =============================================================================
// extractComponentMaskById — pixel mask of an arbitrary component by id
//
// Selecting a component by id via get_edge_of_outer_loop_of_componentBSFace()
// updates the internal m_biggestComponentBSFace to that component; the
// subsequent biggest_component_pixel_mask() then returns that component's mask.
// Must be called after calculateShells() and before releaseVoronoi().
// =============================================================================
std::vector<char> CTImage::extractComponentMaskById(int component_id) {
    if (!m_voronoi) {
        throw VoronoiError(
            "extractComponentMaskById: Voronoi not available. "
            "Call calculateShells() before releaseVoronoi().");
    }

    V::ImageProcessingBC& bc = m_voronoi->bc;

    try {
        bc.get_edge_of_outer_loop_of_componentBSFace(component_id);
        return bc.biggest_component_pixel_mask(m_height, m_width, COORDINATE_OFFSET);
    } catch (const std::exception& e) {
        throw VoronoiError(std::string("extractComponentMaskById failed: ") + e.what());
    }
}

// =============================================================================
// computeBiggestComponentMask — component-only fast path
//
// Runs only: constructForImageProcessing16bit + classifyByComponents +
// select_biggest_componentBSFace + biggest_component_pixel_mask.
// Inner-shell peeling, Mahalanobis, and bed detection are all skipped.
// Voronoi object is released immediately after mask extraction.
// =============================================================================
std::vector<char> CTImage::computeBiggestComponentMask() {
    try {
        // Build Voronoi / beta-complex (same construction as calculateShells())
        buildVoronoi();
        V::ImageProcessingBC& bc = m_voronoi->bc;

        bc.set_beta_value(m_beta);
        bc.classifyByComponents();
        bc.select_biggest_componentBSFace();

        // Build H*W pixel mask
        std::vector<char> mask =
            bc.biggest_component_pixel_mask(m_height, m_width, COORDINATE_OFFSET);

        // Release Voronoi immediately (~340 MB)
        m_voronoi.reset();

        return mask;

    } catch (const std::exception& e) {
        m_voronoi.reset();
        throw VoronoiError(std::string(ErrorMessages::SHELL_CALC_ERROR) + ": " + e.what());
    }
}

// =============================================================================
// releaseRawDicom  — free original DICOM buffer + DcmFileFormat after processing
//
// Frees ~1.5 MB per slice (m_originalImage + m_fileFormat DICOM pixel block).
// Safe to call after calculateShells() + peelSkin() when DICOM export is NOT
// needed.  After this call, applyPeeledToDicom() will throw std::logic_error.
// =============================================================================
void CTImage::releaseRawDicom() {
    // Shrink-to-fit the raw pixel buffer so glibc can reclaim the pages.
    // swap trick guarantees capacity is also released (vs. clear() alone).
    {
        std::vector<uint16_t>().swap(m_originalImage);
    }

    // Release the entire DCMTK DcmFileFormat tree (metadata + pixel data).
    // This is the larger allocation (~1 MB+ for pixel table + DICOM tags).
    m_fileFormat.reset();

    m_rawDicomReleased = true;
}

// =============================================================================
// Cutting point detection
// =============================================================================
int CTImage::calculateCuttingShell(CuttingAlgorithm algo) {
    if (m_shells.size() < 2) {
        m_cutRule = CutRule::None;
        return 0;
    }

    try {
        switch (algo) {
            case CuttingAlgorithm::THREE_SIGMA:
                m_cutRule = CutRule::ThreeSigma;
                return cutting3SigmaDistance();
            case CuttingAlgorithm::MAHAL_BIC: {
                const int cutIndex = cuttingMahalBic();
                // cuttingMahalBic() falls back to the Euclidean rule when there
                // are too few shells for the fit (m_mahalFit.active stays false).
                m_cutRule = m_mahalFit.active ? CutRule::MahalBic : CutRule::EuclideanFallback;
                return cutIndex;
            }
            case CuttingAlgorithm::GMM:
                m_cutRule = CutRule::Euclidean;
                return cuttingGmmSequential();
            case CuttingAlgorithm::EUCLIDEAN:
                m_cutRule = CutRule::Euclidean;
                return cuttingEuclideanDistance();
        }
    } catch (...) {
        m_cutRule = CutRule::EuclideanFallback;
        std::fprintf(stderr,
                     "CTImage::calculateCuttingShell: cutting algorithm %d threw, "
                     "falling back to Euclidean distance\n",
                     static_cast<int>(algo));
        return cuttingEuclideanDistance();
    }
    m_cutRule = CutRule::EuclideanFallback;
    return cuttingEuclideanDistance();
}

std::vector<std::pair<double, double>> CTImage::extractShellStatistics(
    bool excludeOutermost) const {
    std::vector<std::pair<double, double>> pts;
    int startIdx = excludeOutermost ? 1 : 0;
    for (int i = startIdx; i < static_cast<int>(m_shells.size()); ++i) {
        const CTShell& sh = m_shells[i];
        if (!sh.hasStatistics()) {
            m_doNotChangeThickness = true;
            continue;
        }
        pts.emplace_back(sh.mu(), sh.sigma());
    }
    return pts;
}

// =============================================================================
// Cutting algorithms — thin delegation wrappers.
// Shell-statistics extraction (member, has the m_doNotChangeThickness side
// effect) + per-shell bookkeeping (m_shellMahalD/m_shellInside) stay here;
// the actual math is in ShellStatistics (S1 extraction).
// =============================================================================
int CTImage::cuttingEuclideanDistance() {
    auto points = extractShellStatistics();
    const int offset = EXCLUDE_OUTERMOST_SHELL ? 1 : 0;
    return ShellStatistics::cuttingEuclideanDistance(points, offset);
}

int CTImage::cutting3SigmaDistance() {
    auto points = extractShellStatistics();

    // Initialize per-shell vectors (size = m_shells.size())
    const int nShells = static_cast<int>(m_shells.size());
    m_shellMahalD.assign(nShells, -1.0);
    m_shellInside.assign(nShells, false);

    const int offset = EXCLUDE_OUTERMOST_SHELL ? 1 : 0;
    auto result = ShellStatistics::cutting3SigmaDistance(points, offset, SIGMA_THRESHOLD,
                                                         DISTANCE_JUMP_SIGMA);

    if (!result.computed) {
        // points.size() < 4, or singular covariance — same fallback as before.
        return cuttingEuclideanDistance();
    }

    // Store per-point Mahalanobis distance/inside flag into per-shell vectors.
    // points[i] corresponds to m_shells[i + offset].
    for (int i = 0; i < static_cast<int>(result.mahalDistances.size()); ++i) {
        int shellIdx = i + offset;
        if (shellIdx < nShells) {
            m_shellMahalD[shellIdx] = result.mahalDistances[i];
            m_shellInside[shellIdx] = result.inside[i];
        }
    }
    // shell[0] stays at default: mahal=-1, inside=false

    return result.cuttingIndex;
}

int CTImage::cuttingMahalBic() {
    auto points = extractShellStatistics();

    const int nShells = static_cast<int>(m_shells.size());
    m_shellMahalD.assign(nShells, -1.0);
    m_shellInside.assign(nShells, false);
    m_mahalFit = MahalFit{};

    const int offset = EXCLUDE_OUTERMOST_SHELL ? 1 : 0;
    auto result = ShellStatistics::cuttingMahalBic(points, offset);

    if (!result.computed) {
        // Fewer than 4 non-air shells (harness: too_few_shells, no cut defined).
        // Keep the engine's pre-existing fallback for too-few shells.
        return cuttingEuclideanDistance();
    }

    for (int i = 0; i < static_cast<int>(result.mahalDistances.size()); ++i) {
        int shellIdx = i + offset;
        if (shellIdx < nShells) {
            m_shellMahalD[shellIdx] = result.mahalDistances[i];
            m_shellInside[shellIdx] = result.inside[i];
        }
    }

    m_mahalFit.active = true;
    if (result.changepointFound) {
        m_mahalFit.valid = true;
        m_mahalFit.meanMu = result.meanMu;
        m_mahalFit.meanSigma = result.meanSigma;
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 2; ++c)
                m_mahalFit.cov[r][c] = result.cov[r][c];
        m_mahalFit.tau = result.tau;
    }
    return result.cuttingIndex;
}

int CTImage::cuttingGmmSequential() {
    // GMM not yet implemented — fall back to Euclidean
    return cuttingEuclideanDistance();
}

// =============================================================================
// Skin Peeling
// =============================================================================
void CTImage::peelSkin(PeelRegion region) {
    if (m_shells.empty())
        return;

    for (int si = 0; si <= m_peelIndex && si < static_cast<int>(m_shells.size()); ++si) {
        for (const auto& vdata : m_shells[si]) {
            auto [row, col] =
                CoordinateTransform::voronoiToImagePixel(vdata.x, vdata.y, m_height, m_width);

            if (!CoordinateTransform::isValidPixel(row, col, m_height, m_width))
                continue;

            // Region filter (image coordinates: row↓, col→)
            switch (region) {
                case PeelRegion::UpperHalf:
                    if (row >= m_height / 2)
                        continue;
                    break;
                case PeelRegion::LowerHalf:
                    if (row < m_height / 2)
                        continue;
                    break;
                case PeelRegion::LeftHalf:
                    if (col >= m_width / 2)
                        continue;
                    break;
                case PeelRegion::RightHalf:
                    if (col < m_width / 2)
                        continue;
                    break;
                case PeelRegion::WholeBody:
                    break;
            }

            m_image[row * m_width + col] = 0;
            m_originalImage[row * m_width + col] = m_minIntensity;
        }
    }
    m_peeled = true;
}

void CTImage::halfPeelSkin() {
    peelSkin(PeelRegion::RightHalf);
}

// =============================================================================
// Apply peeled pixels back into DCMTK dataset for saving
// =============================================================================
void CTImage::applyPeeledToDicom() {
    if (m_rawDicomReleased)
        throw std::logic_error(
            "CTImage::applyPeeledToDicom(): raw DICOM buffers have been released "
            "via releaseRawDicom(). Cannot save DICOM after raw data is freed.");

    DcmDataset* ds = m_fileFormat->getDataset();
    ds->putAndInsertUint16Array(DCM_PixelData, m_originalImage.data(),
                                static_cast<unsigned long>(m_originalImage.size()));
}

// =============================================================================
// Statistics snapshot
// =============================================================================
CTImage::Stats CTImage::collectStats() const {
    Stats s;
    s.filename = m_filepath;
    s.peelIndex = m_peelIndex;
    s.shellCount = static_cast<int>(m_shells.size());
    s.isPeeled = m_peeled;

    if (!m_shells.empty() && m_peelIndex < static_cast<int>(m_shells.size())) {
        const CTShell& sh = m_shells[m_peelIndex];
        if (sh.hasStatistics()) {
            s.mu = sh.mu();
            s.sigma = sh.sigma();
            s.hasStats = true;
        }
    }
    return s;
}

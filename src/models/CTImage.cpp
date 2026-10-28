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

#include "utils/CoordinateTransform.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <cassert>
#include <cstdio>

// =============================================================================
// Internal Voronoi wrapper  (keeps V headers out of CTImage.h)
// Stores outer-shell QuasiVertex objects so that calculateInnerShellsAdaptive()
// can use them as seeds without re-invoking get_edge_of_outer_loop_of_biggest_componentBSFace().
// =============================================================================
#include "QuasiVertex.h"
#include "QuasiEdge.h"

struct CTImage::VoronoiImpl {
    V::ImageProcessingBC bc;
    std::vector<V::QuasiVertex> outerShellQVerts; // populated by extractOuterShell()
};

// =============================================================================
// Factory
// =============================================================================
CTImage CTImage::fromDicomFile(const std::string& filepath, double beta, int defaultThickness)
{
    // Register JPEG decoders once (idempotent — DCMTK ignores repeated calls
    // when codecs are already registered). This enables Transfer Syntax
    // 1.2.840.10008.1.2.4.70 (JPEG Lossless) and all other JPEG processes.
    // Thread-safe: DCMTK uses an internal mutex for codec registration.
    static std::once_flag s_jpegCodecFlag;
    std::call_once(s_jpegCodecFlag, []() {
        DJDecoderRegistration::registerCodecs();
    });

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
CTImage::CTImage(std::unique_ptr<DcmFileFormat> fileFormat,
                 const std::string& filepath,
                 double beta,
                 int    defaultThickness)
    : m_fileFormat(std::move(fileFormat))
    , m_filepath(filepath)
    , m_beta(beta)
    , m_defaultThickness(defaultThickness)
{
    extractImageFromDicom();
}

CTImage::~CTImage() = default;
CTImage::CTImage(CTImage&&) noexcept = default;
CTImage& CTImage::operator=(CTImage&&) noexcept = default;

// =============================================================================
// DICOM Image Extraction
// Matches Python: slope*pixel_array + intercept → apply_voi_lut → subtract min
// Bug 5 fix: explicitly read Window Center / Window Width from DICOM tags and
// pass them to DicomImage::setWindow() so the correct preset is used.
// Without this, DCMTK may default to the full HU range (-1024..3071), making
// the image appear very low-contrast (washed out / grey).
// =============================================================================
void CTImage::extractImageFromDicom()
{
    DcmDataset* ds = m_fileFormat->getDataset();

    // 1. Dimensions
    Uint16 rows = 0, cols = 0;
    ds->findAndGetUint16(DCM_Rows,    rows);
    ds->findAndGetUint16(DCM_Columns, cols);
    m_height = static_cast<int>(rows);
    m_width  = static_cast<int>(cols);
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
            ds->findAndGetElement(DCM_WindowWidth,  wwElem).good() && wwElem)
        {
            Float64 wc = 0.0, ww = 0.0;
            // Use the FIRST value (index 0) — matches pydicom default behaviour
            if (wcElem->getFloat64(wc, 0).good() &&
                wwElem->getFloat64(ww, 0).good() && ww > 0.0)
            {
                dicomImg.setWindow(wc, ww);
            }
        }
        // If no window tags: DCMTK uses auto-window from image statistics (best fallback)
    }

    // 5. Get 16-bit output with the window applied
    const uint16_t* outPtr =
        static_cast<const uint16_t*>(dicomImg.getOutputData(16));
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
                const Uint16* rawData =
                    static_cast<const Uint16*>(diPixel->getData());
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
                const int16_t* sPtr =
                    reinterpret_cast<const int16_t*>(m_originalImage.data());
                const int16_t* sEnd = sPtr + m_originalImage.size();
                int16_t sMin = *std::min_element(sPtr, sEnd);
                // Store as the uint16 bit pattern (memcpy is well-defined)
                uint16_t uMin = 0;
                static_assert(sizeof(int16_t) == sizeof(uint16_t), "size mismatch");
                std::memcpy(&uMin, &sMin, sizeof(uint16_t));
                m_minIntensity = uMin;
            } else {
                // Unsigned: straightforward
                m_minIntensity = *std::min_element(
                    m_originalImage.begin(), m_originalImage.end());
            }
        } else {
            m_minIntensity = 0;
        }
    }

    // 6. Subtract minimum — matches Python: image -= np.min(image)
    uint16_t minVal = *std::min_element(m_image.begin(), m_image.end());
    for (auto& p : m_image) p -= minVal;

    m_imageMax = static_cast<double>(
        *std::max_element(m_image.begin(), m_image.end()));

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
void CTImage::normalizeToMax(double globalMax)
{
    if (globalMax <= 0.0) globalMax = 1.0;
    for (auto& p : m_image)
        p = static_cast<uint16_t>(IMAGE_MAX_VALUE * static_cast<double>(p) / globalMax);
}

void CTImage::setBeta(double b)
{
    m_beta = std::max(0.0, std::min(1.0, b));
    m_shells.clear();
    m_voronoi.reset();

    // All of the following are derived from m_shells/the beta-complex built
    // above, so they go stale the moment beta changes and must be recomputed
    // by the next calculateShells() call. Leaving them behind previously let
    // callers read peel/cut/bed state computed under the OLD beta.
    m_peelIndex     = 0;
    m_algoPeelIndex = 0;
    m_peeled        = false;
    m_shellMahalD.clear();
    m_shellInside.clear();
    m_bedResult = BedDetector::Result{};
}

// =============================================================================
// calculateShells  — main entry point for Voronoi + shell extraction
// Always extracts all reachable shells (maximize mode). See CTImage.h for rationale.
// =============================================================================
void CTImage::calculateShells()
{
    m_timedOut = false;
    try {
        // Build Voronoi / beta-complex
        m_voronoi = std::make_unique<VoronoiImpl>();
        V::ImageProcessingBC& bc = m_voronoi->bc;

        // Transpose image to column-major for the C++ library
        std::vector<std::vector<int>> colorDataSet(
            m_width, std::vector<int>(m_height));
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
        bc.set_beta_value(m_beta);
        bc.classifyByComponents();

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
                cacheHit = BedCache::load(cacheDir, m_beta, BedCache::ALGORITHM_VERSION,
                                          m_filepath, m_bedResult);
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
                    vi.id          = ci.id;
                    vi.centroid_x  = ci.centroid_x;
                    vi.centroid_y  = ci.centroid_y;
                    vi.area        = ci.area;
                    vi.circularity = ci.circularity;
                    vi.score       = ci.score;
                    vd_comps.push_back(vi);
                }

                // body0_cy: mean y of shell[0] vertices (Voronoi space)
                double body0_cy = 0.0;
                if (!m_shells.empty() && !m_shells[0].isEmpty()) {
                    for (const auto& v : m_shells[0]) body0_cy += v.y;
                    body0_cy /= static_cast<double>(m_shells[0].size());
                }

                BedDetector bd;
                m_bedResult = bd.detect(vd_comps, m_image.data(), m_width, m_height,
                                        body0_cy, BedDetector::Params{});

                // Cache save (single slice upsert)
                BedCache::save(cacheDir, m_beta, BedCache::ALGORITHM_VERSION,
                               m_filepath, m_bedResult);
            }

            // --- body0 replacement ---
            const int new_id = m_bedResult.new_body0_component_id;

            if (new_id != -1 && new_id != current_body0_id) {
                auto new_outer_edges = bc.get_edge_of_outer_loop_of_componentBSFace(new_id);
                if (!new_outer_edges.empty()) {
                    // Rebuild shell[0] and outerShellQVerts from new outer loop
                    m_shells.clear();
                    m_voronoi->outerShellQVerts.clear();

                    CTShell newShell;
                    for (auto& edge : new_outer_edges) {
                        auto sv = edge.get_start_quasi_vertex();
                        auto ev = edge.get_end_quasi_vertex();

                        if (!sv.is_visited()) {
                            sv.set_visited(true);
                            auto pt = sv.get_point();
                            newShell.addVertex(pt.getX(), pt.getY(), sv.get_radius());
                            m_voronoi->outerShellQVerts.push_back(sv);
                        }
                        if (!ev.is_visited()) {
                            ev.set_visited(true);
                            auto pt = ev.get_point();
                            newShell.addVertex(pt.getX(), pt.getY(), ev.get_radius());
                            m_voronoi->outerShellQVerts.push_back(ev);
                        }
                    }
                    newShell.calculateStatistics();
                    m_shells.push_back(std::move(newShell));
                }
                // m_image not modified, VD not rebuilt
            }
        }
        // =====================================================================
        // End bed removal hook
        // =====================================================================

        calculateInnerShellsAdaptive(true);
        m_algoPeelIndex   = calculateCuttingShell();
        m_peelIndex       = m_algoPeelIndex;

    } catch (const std::exception& e) {
        throw VoronoiError(std::string(ErrorMessages::SHELL_CALC_ERROR) + ": " + e.what());
    }
}

// =============================================================================
// extractOuterShell
// Calls get_edge_of_outer_loop_of_biggest_componentBSFace() exactly once.
// Collected QuasiVertex objects are cached in m_voronoi->outerShellQVerts so
// that calculateInnerShellsAdaptive() can use them as seeds without repeating
// the expensive biggest-component search.
// =============================================================================
void CTImage::extractOuterShell()
{
    assert(m_voronoi);
    V::ImageProcessingBC& bc = m_voronoi->bc;

    auto outerEdges = bc.get_edge_of_outer_loop_of_biggest_componentBSFace();

    CTShell shell;
    m_voronoi->outerShellQVerts.clear();

    for (auto& edge : outerEdges) {
        auto sv = edge.get_start_quasi_vertex();
        auto ev = edge.get_end_quasi_vertex();

        if (!sv.is_visited()) {
            sv.set_visited(true);
            auto pt = sv.get_point();
            shell.addVertex(pt.getX(), pt.getY(), sv.get_radius());
            m_voronoi->outerShellQVerts.push_back(sv);
        }
        if (!ev.is_visited()) {
            ev.set_visited(true);
            auto pt = ev.get_point();
            shell.addVertex(pt.getX(), pt.getY(), ev.get_radius());
            m_voronoi->outerShellQVerts.push_back(ev);
        }
    }
    shell.calculateStatistics();
    m_shells.push_back(std::move(shell));
}

// =============================================================================
// calculateSingleInnerShell
// Traverses incident edges of the last shell's vertices to find the next layer.
// =============================================================================
void CTImage::calculateSingleInnerShell()
{
    if (m_shells.empty() || !m_voronoi) return;

    const CTShell& currentShell = m_shells.back();
    CTShell newShell;

    for (const auto& vdata : currentShell) {
        // We need the QuasiVertex to call find_incident_quasi_edges().
        // Problem: we stored plain VertexData, not QuasiVertex.
        // Solution: the vertex visited-flag is set on the underlying VertexBU2D,
        // so we look up vertices from the Voronoi object by traversing all beta
        // vertices and matching by coordinate.
        //
        // However this would be O(N^2). Better solution used in practice:
        // Store QuasiVertex objects while the VoronoiImpl is alive, and keep a
        // parallel list alongside VertexData for traversal. This is handled via
        // m_voronoi->pendingVertices populated during shell extraction.
        (void)vdata; // see VoronoiImpl::pendingVertices below
    }
    // Actual traversal is done via stored QuasiVertex references — see below.
    (void)newShell;
}

// =============================================================================
// Inner shell implementation using stored QuasiVertex references
// =============================================================================
void CTImage::calculateInnerShells(int thickness)
{
    if (!m_voronoi) return;
    for (int s = static_cast<int>(m_shells.size()); s < thickness; ++s)
        calculateSingleInnerShell();
}

// Shell calculation with proper QuasiVertex tracking

void CTImage::calculateInnerShellsAdaptive(bool maximize)
{
    if (!m_voronoi) return;

    // Time limit safety: record start time
    const auto calcStartTime = std::chrono::steady_clock::now();
    auto elapsedSeconds = [&calcStartTime]() -> double {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - calcStartTime).count();
    };

    // Use the outer-shell QuasiVertex list cached by extractOuterShell().
    // Ping-pong: only keep the previous shell's QVerts as seeds for the next.
    std::vector<V::QuasiVertex> prevQVerts = m_voronoi->outerShellQVerts;

    // Incremental shell statistics for adaptive stopping (avoid repeated full traversal)
    std::vector<std::pair<double,double>> statsPoints;
    if (EXCLUDE_OUTERMOST_SHELL && !m_shells.empty() && m_shells[0].hasStatistics()) {
        // shell[0] excluded from stats — start empty
    }
    for (int si = (EXCLUDE_OUTERMOST_SHELL ? 1 : 0);
         si < static_cast<int>(m_shells.size()); ++si) {
        const CTShell& sh = m_shells[si];
        if (sh.hasStatistics())
            statsPoints.emplace_back(sh.mu(), sh.sigma());
    }

    // Shared lambda: expand one shell from prevQVerts
    auto expandOneShell = [&](std::vector<V::QuasiVertex>& newQVerts) -> CTShell {
        CTShell newShell;
        for (const auto& vert : prevQVerts) {
            V::QuasiVertex v = vert;
            auto incEdges = v.find_incident_quasi_edges();

            for (auto& edge : incEdges) {
                int state = edge.get_bounding_state(m_beta);
                if (state == V::REGULAR_STATE || state == V::INTERIOR_STATE) {
                    auto sv = edge.get_start_quasi_vertex();
                    auto ev = edge.get_end_quasi_vertex();

                    if (!sv.is_visited()) {
                        sv.set_visited(true);
                        auto pt = sv.get_point();
                        newShell.addVertex(pt.getX(), pt.getY(), sv.get_radius());
                        newQVerts.push_back(std::move(sv));
                    }
                    if (!ev.is_visited()) {
                        ev.set_visited(true);
                        auto pt = ev.get_point();
                        newShell.addVertex(pt.getX(), pt.getY(), ev.get_radius());
                        newQVerts.push_back(std::move(ev));
                    }
                }
            }
        }
        return newShell;
    };

    // Calculate minimum shells first
    for (int i = static_cast<int>(m_shells.size()); i < ADAPTIVE_SHELL_MIN; ++i) {
        if (prevQVerts.empty()) break;
        if (elapsedSeconds() > ADAPTIVE_TIME_LIMIT_SECONDS) { m_timedOut = true; break; }

        std::vector<V::QuasiVertex> newQVerts;
        CTShell newShell = expandOneShell(newQVerts);

        if (!newShell.isEmpty()) {
            newShell.calculateStatistics();
            if (newShell.hasStatistics())
                statsPoints.emplace_back(newShell.mu(), newShell.sigma());
            m_shells.push_back(std::move(newShell));
            prevQVerts = std::move(newQVerts);
        } else {
            break;
        }
    }

    // Adaptive stopping: check every ADAPTIVE_CHECK_INTERVAL shells
    int consecutiveInside = 0;

    for (int shellIdx = static_cast<int>(m_shells.size());
         shellIdx < ADAPTIVE_SHELL_MAX; ++shellIdx)
    {
        if (prevQVerts.empty()) break;

        std::vector<V::QuasiVertex> newQVerts;
        CTShell newShell = expandOneShell(newQVerts);

        if (newShell.isEmpty()) break;

        // Time limit check (primary safety mechanism)
        if (elapsedSeconds() > ADAPTIVE_TIME_LIMIT_SECONDS) {
            m_timedOut = true;
            break;
        }

        newShell.calculateStatistics();
        if (newShell.hasStatistics())
            statsPoints.emplace_back(newShell.mu(), newShell.sigma());
        m_shells.push_back(std::move(newShell));
        prevQVerts = std::move(newQVerts);

        // Check adaptive stopping every N shells (using incremental statsPoints)
        if (shellIdx % ADAPTIVE_CHECK_INTERVAL == 0 && statsPoints.size() >= 4) {
            const double np = static_cast<double>(statsPoints.size());
            double meanMu = 0.0, meanSigma = 0.0;
            for (auto& [mu, sigma] : statsPoints) {
                meanMu    += mu;
                meanSigma += sigma;
            }
            meanMu    /= np;
            meanSigma /= np;

            double c00 = 0, c01 = 0, c11 = 0;
            for (auto& [mu, sigma] : statsPoints) {
                double dm = mu    - meanMu;
                double ds = sigma - meanSigma;
                c00 += dm * dm;
                c01 += dm * ds;
                c11 += ds * ds;
            }
            const double div = np - 1.0;
            double cov[2][2] = {{c00/div, c01/div}, {c01/div, c11/div}};
            double inv[2][2];

            if (invert2x2(cov, inv)) {
                const auto& [lmu, lsig] = statsPoints.back();
                double diff[2] = {lmu - meanMu, lsig - meanSigma};
                double mahal = mahalanobis2D(diff, inv);

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

void CTImage::releaseVoronoi()
{
    m_voronoi.reset();
}

// =============================================================================
// extractBiggestComponentMaskFromVoronoi — reuse existing Voronoi state
//
// Must be called after calculateShells() and before releaseVoronoi().
// Calls select_biggest_componentBSFace() again to ensure state is correct,
// then extracts the pixel mask via biggest_component_pixel_mask().
// =============================================================================
std::vector<char> CTImage::extractBiggestComponentMaskFromVoronoi()
{
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

        std::vector<char> mask = bc.biggest_component_pixel_mask(
            m_height, m_width, COORDINATE_OFFSET);

        return mask;

    } catch (const std::exception& e) {
        throw VoronoiError(
            std::string("extractBiggestComponentMaskFromVoronoi failed: ") + e.what());
    }
}

// =============================================================================
// enumerateComponentScores — expose all β-complex component scores
//
// Must be called after calculateShells() and before releaseVoronoi().
// Returns score-descending sorted vector (mirrors ImageProcessingBC::enumerate_components).
// =============================================================================
std::vector<CTImage::ComponentScoreInfo> CTImage::enumerateComponentScores()
{
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
        info.id                = ci.id;
        info.centroid_x        = ci.centroid_x;
        info.centroid_y        = ci.centroid_y;
        info.area              = ci.area;
        info.circularity       = ci.circularity;
        info.score             = ci.score;
        info.perimeter_total   = ci.perimeter_total;
        info.outer_loop_length = ci.outer_loop_length;
        info.n_inner_loops     = ci.n_inner_loops;
        info.n_outer_edges     = ci.n_outer_edges;
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
std::vector<char> CTImage::extractComponentMaskById(int component_id)
{
    if (!m_voronoi) {
        throw VoronoiError(
            "extractComponentMaskById: Voronoi not available. "
            "Call calculateShells() before releaseVoronoi().");
    }

    V::ImageProcessingBC& bc = m_voronoi->bc;

    try {
        bc.get_edge_of_outer_loop_of_componentBSFace(component_id);
        return bc.biggest_component_pixel_mask(
            m_height, m_width, COORDINATE_OFFSET);
    } catch (const std::exception& e) {
        throw VoronoiError(
            std::string("extractComponentMaskById failed: ") + e.what());
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
std::vector<char> CTImage::computeBiggestComponentMask()
{
    // Build Voronoi / beta-complex (same as calculateShells start)
    m_voronoi = std::make_unique<VoronoiImpl>();
    V::ImageProcessingBC& bc = m_voronoi->bc;

    // Transpose image to column-major (required by V library)
    std::vector<std::vector<int>> colorDataSet(
        m_width, std::vector<int>(m_height));
    for (int r = 0; r < m_height; ++r)
        for (int c = 0; c < m_width; ++c)
            colorDataSet[c][r] = static_cast<int>(m_image[r * m_width + c]);

    // Read BitsStored (same logic as calculateShells)
    Uint16 bitsStored = 16;
    if (m_fileFormat && m_fileFormat->getDataset())
        m_fileFormat->getDataset()->findAndGetUint16(DCM_BitsStored, bitsStored);
    const int bitsStoredInt = static_cast<int>(bitsStored);

    try {
        bc.constructForImageProcessing16bit(colorDataSet, bitsStoredInt, DARKER_PIXEL_BIGGER_DISK);
        bc.set_beta_value(m_beta);
        bc.classifyByComponents();
        bc.select_biggest_componentBSFace();

        // Build H*W pixel mask
        std::vector<char> mask = bc.biggest_component_pixel_mask(
            m_height, m_width, COORDINATE_OFFSET);

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
void CTImage::releaseRawDicom()
{
    // Shrink-to-fit the raw pixel buffer so glibc can reclaim the pages.
    // swap trick guarantees capacity is also released (vs. clear() alone).
    { std::vector<uint16_t>().swap(m_originalImage); }

    // Release the entire DCMTK DcmFileFormat tree (metadata + pixel data).
    // This is the larger allocation (~1 MB+ for pixel table + DICOM tags).
    m_fileFormat.reset();

    m_rawDicomReleased = true;
}

// =============================================================================
// Cutting point detection
// =============================================================================
int CTImage::calculateCuttingShell(CuttingAlgorithm algo)
{
    if (m_shells.size() < 2) return 0;

    try {
        switch (algo) {
            case CuttingAlgorithm::THREE_SIGMA:  return cutting3SigmaDistance();
            case CuttingAlgorithm::GMM:          return cuttingGmmSequential();
            case CuttingAlgorithm::EUCLIDEAN:    return cuttingEuclideanDistance();
        }
    } catch (...) {
        return cuttingEuclideanDistance();
    }
    return cuttingEuclideanDistance();
}

std::vector<std::pair<double,double>>
CTImage::extractShellStatistics(bool excludeOutermost) const
{
    std::vector<std::pair<double,double>> pts;
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

int CTImage::cuttingEuclideanDistance()
{
    auto points = extractShellStatistics();
    if (points.empty()) return 0;

    double maxDist  = -1.0;
    int    maxIndex = 0;

    for (int i = 0; i + 1 < static_cast<int>(points.size()); ++i) {
        double dmu    = points[i+1].first  - points[i].first;
        double dsigma = points[i+1].second - points[i].second;
        double dist   = std::sqrt(dmu*dmu + dsigma*dsigma);
        if (dist > maxDist) {
            maxDist  = dist;
            maxIndex = i;
        }
    }

    return maxIndex + (EXCLUDE_OUTERMOST_SHELL ? 1 : 0);
}

bool CTImage::invert2x2(const double cov[2][2], double inv[2][2])
{
    double det = cov[0][0]*cov[1][1] - cov[0][1]*cov[1][0];
    if (std::abs(det) < 1e-10) return false;
    inv[0][0] =  cov[1][1] / det;
    inv[0][1] = -cov[0][1] / det;
    inv[1][0] = -cov[1][0] / det;
    inv[1][1] =  cov[0][0] / det;
    return true;
}

double CTImage::mahalanobis2D(const double diff[2], const double inv[2][2])
{
    double t0 = inv[0][0]*diff[0] + inv[0][1]*diff[1];
    double t1 = inv[1][0]*diff[0] + inv[1][1]*diff[1];
    return std::sqrt(diff[0]*t0 + diff[1]*t1);
}

int CTImage::cutting3SigmaDistance()
{
    auto points = extractShellStatistics();

    // Initialize per-shell vectors (size = m_shells.size())
    const int nShells = static_cast<int>(m_shells.size());
    m_shellMahalD.assign(nShells, -1.0);
    m_shellInside.assign(nShells, false);

    if (static_cast<int>(points.size()) < 4) return cuttingEuclideanDistance();

    const double np = static_cast<double>(points.size());
    double meanMu = 0, meanSigma = 0;
    for (auto& [mu, sigma] : points) { meanMu += mu; meanSigma += sigma; }
    meanMu /= np; meanSigma /= np;

    // Sample covariance (N-1) to match np.cov
    double c00 = 0, c01 = 0, c11 = 0;
    for (auto& [mu, sigma] : points) {
        double dm = mu - meanMu, ds = sigma - meanSigma;
        c00 += dm*dm; c01 += dm*ds; c11 += ds*ds;
    }
    double div = np - 1.0;
    double cov[2][2] = {{c00/div, c01/div}, {c01/div, c11/div}};
    double inv[2][2];

    if (!invert2x2(cov, inv)) return cuttingEuclideanDistance();

    std::vector<bool> inside(points.size());
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        double diff[2] = {points[i].first - meanMu, points[i].second - meanSigma};
        double d = mahalanobis2D(diff, inv);
        inside[i] = (d <= SIGMA_THRESHOLD);

        // Store in per-shell vectors.
        // points[i] corresponds to m_shells[i + offset] where offset = 1 if EXCLUDE_OUTERMOST.
        int shellIdx = i + (EXCLUDE_OUTERMOST_SHELL ? 1 : 0);
        if (shellIdx < nShells) {
            m_shellMahalD[shellIdx] = d;
            m_shellInside[shellIdx] = inside[i];
        }
    }
    // shell[0] stays at default: mahal=-1, inside=false

    const int offset = EXCLUDE_OUTERMOST_SHELL ? 1 : 0;

    for (int i = 0; i < static_cast<int>(inside.size()); ++i) {
        if (inside[i]) {
            if (i == 0) return findDistanceJump(points);
            return (i - 1) + offset;
        }
    }
    // All outside → distance jump
    return findDistanceJump(points);
}

int CTImage::findDistanceJump(const std::vector<std::pair<double,double>>& points)
{
    if (points.size() < 2) return 0;

    std::vector<double> dists;
    for (int i = 0; i + 1 < static_cast<int>(points.size()); ++i) {
        double dmu = points[i+1].first - points[i].first;
        double ds  = points[i+1].second - points[i].second;
        dists.push_back(std::sqrt(dmu*dmu + ds*ds));
    }

    double dmean = 0;
    for (double d : dists) dmean += d;
    dmean /= static_cast<double>(dists.size());

    double dvar = 0;
    for (double d : dists) dvar += (d-dmean)*(d-dmean);
    double dstd = std::sqrt(dvar / static_cast<double>(dists.size()));

    double threshold = dmean + DISTANCE_JUMP_SIGMA * dstd;
    int offset = EXCLUDE_OUTERMOST_SHELL ? 1 : 0;

    for (int i = 0; i < static_cast<int>(dists.size()); ++i) {
        if (dists[i] > threshold) return i + offset;
    }
    return EXCLUDE_OUTERMOST_SHELL ? 1 : 0;
}

int CTImage::cuttingGmmSequential()
{
    // GMM not yet implemented — fall back to Euclidean
    return cuttingEuclideanDistance();
}

// =============================================================================
// Skin Peeling
// =============================================================================
void CTImage::peelSkin(PeelRegion region)
{
    if (m_shells.empty()) return;

    for (int si = 0; si <= m_peelIndex && si < static_cast<int>(m_shells.size()); ++si) {
        for (const auto& vdata : m_shells[si]) {
            auto [row, col] = CoordinateTransform::voronoiToImagePixel(
                vdata.x, vdata.y, m_height, m_width);

            if (!CoordinateTransform::isValidPixel(row, col, m_height, m_width))
                continue;

            // Region filter (image coordinates: row↓, col→)
            switch (region) {
            case PeelRegion::UpperHalf: if (row >= m_height / 2) continue; break;
            case PeelRegion::LowerHalf: if (row <  m_height / 2) continue; break;
            case PeelRegion::LeftHalf:  if (col >= m_width  / 2) continue; break;
            case PeelRegion::RightHalf: if (col <  m_width  / 2) continue; break;
            case PeelRegion::WholeBody: break;
            }

            m_image[row * m_width + col]         = 0;
            m_originalImage[row * m_width + col] = m_minIntensity;
        }
    }
    m_peeled = true;
}

void CTImage::halfPeelSkin()
{
    peelSkin(PeelRegion::RightHalf);
}

// =============================================================================
// Apply peeled pixels back into DCMTK dataset for saving
// =============================================================================
void CTImage::applyPeeledToDicom()
{
    if (m_rawDicomReleased)
        throw std::logic_error(
            "CTImage::applyPeeledToDicom(): raw DICOM buffers have been released "
            "via releaseRawDicom(). Cannot save DICOM after raw data is freed.");

    DcmDataset* ds = m_fileFormat->getDataset();
    ds->putAndInsertUint16Array(
        DCM_PixelData,
        m_originalImage.data(),
        static_cast<unsigned long>(m_originalImage.size()));
}

// =============================================================================
// Statistics snapshot
// =============================================================================
CTImage::Stats CTImage::collectStats() const
{
    Stats s;
    s.filename   = m_filepath;
    s.peelIndex  = m_peelIndex;
    s.shellCount = static_cast<int>(m_shells.size());
    s.isPeeled   = m_peeled;

    if (!m_shells.empty() && m_peelIndex < static_cast<int>(m_shells.size())) {
        const CTShell& sh = m_shells[m_peelIndex];
        if (sh.hasStatistics()) {
            s.mu       = sh.mu();
            s.sigma    = sh.sigma();
            s.hasStats = true;
        }
    }
    return s;
}

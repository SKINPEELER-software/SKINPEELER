#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <stdexcept>
#include <chrono>

#include "CTShell.h"
#include "BedDetector.h"
#include "config/Settings.h"

// Forward declaration — avoid pulling heavy DCMTK/V headers into every TU
class DcmFileFormat;

// =============================================================================
// CTImage
//
// Represents one CT slice. Manages:
//   - DICOM metadata and raw pixel data  (via DCMTK)
//   - Processed display image            (slope/intercept + VOI LUT, uint16)
//   - Voronoi/beta-complex calculation   (via ImageProcessingBC)
//   - Shell extraction and statistics
//   - Skin peeling operation
// =============================================================================
class CTImage {
public:
    // -------------------------------------------------------------------------
    // Exceptions
    // -------------------------------------------------------------------------
    struct DicomLoadError   : std::runtime_error { using std::runtime_error::runtime_error; };
    struct VoronoiError     : std::runtime_error { using std::runtime_error::runtime_error; };
    struct PeelingError     : std::runtime_error { using std::runtime_error::runtime_error; };

    // -------------------------------------------------------------------------
    // Factory
    // -------------------------------------------------------------------------
    static CTImage fromDicomFile(
        const std::string& filepath,
        double beta               = DEFAULT_BETA,
        int    defaultThickness   = DEFAULT_SHELL_THICKNESS);

    // -------------------------------------------------------------------------
    // Constructors / destructor
    // -------------------------------------------------------------------------
    CTImage(std::unique_ptr<DcmFileFormat> fileFormat,
            const std::string& filepath,
            double beta,
            int    defaultThickness);
    ~CTImage();

    // Non-copyable, movable
    // Move ops declared here, defined in .cpp where VoronoiImpl is complete
    CTImage(const CTImage&)            = delete;
    CTImage& operator=(const CTImage&) = delete;
    CTImage(CTImage&&)                 noexcept;
    CTImage& operator=(CTImage&&)      noexcept;

    // -------------------------------------------------------------------------
    // Properties
    // -------------------------------------------------------------------------
    const std::string& filepath() const   { return m_filepath; }
    int width()  const                    { return m_width; }
    int height() const                    { return m_height; }

    // Processed display image (uint16, row-major, height x width)
    const std::vector<uint16_t>& image() const { return m_image; }

    // Raw DICOM pixels (for export after peeling)
    const std::vector<uint16_t>& originalImage() const { return m_originalImage; }

    double imageMax() const { return m_imageMax; }
    double sliceLocation() const { return m_sliceLocation; }

    const std::vector<CTShell>& shells() const { return m_shells; }
    std::vector<CTShell>&       shells()       { return m_shells; }

    int peelIndex() const         { return m_peelIndex; }
    void setPeelIndex(int idx)    { m_peelIndex = std::max(0, idx); }

    double beta() const           { return m_beta; }
    void setBeta(double b);

    bool isPeeled()              const { return m_peeled; }
    bool hasShells()             const { return !m_shells.empty(); }
    bool doNotChangeThickness()  const { return m_doNotChangeThickness; }
    bool timedOut()              const { return m_timedOut; }
    int  algoPeelIndex()         const { return m_algoPeelIndex; }

    // Per-shell Mahalanobis distance and inside/outside from cutting3SigmaDistance().
    // Index 0 = shell[0] (excluded from stats, mahal=-1), index 1+ = shell[1]+.
    const std::vector<double>& shellMahalD()  const { return m_shellMahalD; }
    const std::vector<bool>&   shellInside()  const { return m_shellInside; }

    // -------------------------------------------------------------------------
    // Bed removal control
    // Default off — when disabled, calculateShells() behaves exactly as before.
    // -------------------------------------------------------------------------
    void setBedRemoval(bool enabled)             { m_bedRemovalEnabled = enabled; }
    bool bedRemovalEnabled() const               { return m_bedRemovalEnabled; }
    void setSpecimenDir(const std::string& d)    { m_specimenDir = d; }
    const std::string& specimenDir() const       { return m_specimenDir; }
    const BedDetector::Result& bedResult() const { return m_bedResult; }

    // -------------------------------------------------------------------------
    // Operations
    // -------------------------------------------------------------------------

    // Normalize display image to a global maximum (cross-slice consistency)
    void normalizeToMax(double globalMax);

    // Core: build Voronoi, extract all reachable shells, determine cutting point.
    // Always maximizes shell extraction — there is no partial mode.
    // Caller should call releaseVoronoi() after this to free ~340 MB per slice.
    // Design decision: VD construction dominates cost (~1.7s, ~340MB). Extracting
    // all shells adds negligible overhead (~1.3MB for ~96 shells) but eliminates
    // the need to rebuild VD if full shell data is needed later (e.g. analysis).
    void calculateShells();

    // Zero out pixels in specified region (default: whole body)
    void peelSkin(PeelRegion region = PeelRegion::WholeBody);

    // Right-half peeling only (delegates to peelSkin(PeelRegion::RightHalf))
    void halfPeelSkin();

    // Write peeled raw pixel data back into the DcmFileFormat for saving
    void applyPeeledToDicom();

    // Release Voronoi object to free memory (safe after shells extracted)
    void releaseVoronoi();

    // Release raw DICOM buffers to free ~1.5 MB per slice after shell extraction.
    // Safe to call once calculateShells() and peelSkin() are both complete and
    // applyPeeledToDicom() / save_dicom() will NOT be called afterwards.
    // After this call: originalImage() returns empty vector; dcmFileFormat()
    // returns the DcmFileFormat with PixelData already removed (cannot save).
    // Calling releaseRawDicom() then applyPeeledToDicom() is a programming error
    // and will throw std::logic_error.
    // Typical use: analysis-only sessions (Jaccard, statistics) where DICOM
    // export is never needed.
    void releaseRawDicom();

    // -------------------------------------------------------------------------
    // computeBiggestComponentMask — component-only fast path (cut-independent)
    //
    // Builds Voronoi/beta-complex, runs classifyByComponents +
    // select_biggest_componentBSFace, converts the resulting vertex set to a
    // flat H*W pixel mask (1=body, 0=background), then immediately releases the
    // Voronoi object to free ~340 MB.
    //
    // Does NOT run inner-shell peeling, Mahalanobis classification, or bed
    // detection.  Purpose: cut-independent topological continuity metric (F3).
    //
    // Returns: flat vector<char> of size height()*width(), row-major, 1=body.
    // Throws VoronoiError if Voronoi construction fails.
    // -------------------------------------------------------------------------
    std::vector<char> computeBiggestComponentMask();

    // -------------------------------------------------------------------------
    // extractBiggestComponentMaskFromVoronoi — extract component mask from
    // already-constructed Voronoi (after calculateShells(), before releaseVoronoi())
    //
    // Reuses the existing m_voronoi state built by calculateShells().
    // Equivalent result to computeBiggestComponentMask() but avoids rebuilding
    // Voronoi when shells have already been computed.
    //
    // Must be called AFTER calculateShells() and BEFORE releaseVoronoi().
    // Throws VoronoiError if m_voronoi is not available (already released or
    // calculateShells() was not called).
    //
    // Returns: flat vector<char> of size height()*width(), row-major, 1=body.
    // -------------------------------------------------------------------------
    std::vector<char> extractBiggestComponentMaskFromVoronoi();

    // -------------------------------------------------------------------------
    // ComponentScoreInfo — per-component diagnostic info for score analysis.
    // Mirrors ImageProcessingBC::ComponentInfo (additive, no V headers exposed).
    // -------------------------------------------------------------------------
    struct ComponentScoreInfo {
        int    id;
        double centroid_x;
        double centroid_y;
        double area;
        double circularity;
        double score;        // area * circularity^2 (same as biggest-component selection)
        double perimeter_total   = 0.0;  // Σ all regular boundary edges (outer + inner-hole rims)
        double outer_loop_length = 0.0;  // outer loop only
        int    n_inner_loops     = 0;
        int    n_outer_edges     = 0;
    };

    // -------------------------------------------------------------------------
    // enumerateComponentScores — enumerate all β-complex components with scores.
    //
    // Must be called AFTER calculateShells() and BEFORE releaseVoronoi().
    // Returns a score-descending sorted vector (index 0 = highest-score component,
    // i.e. the one that was selected as body0).
    // Throws VoronoiError if m_voronoi is not available.
    // -------------------------------------------------------------------------
    // Non-const: fills geometry fields via ImageProcessingBC::enumerate_components_with_geometry()
    // (calls computePerimeter/computeLength which set m_perimeter/m_length on components).
    std::vector<ComponentScoreInfo> enumerateComponentScores();

    // extractComponentMaskById — pixel mask of an arbitrary β-complex component
    // by id (from enumerateComponentScores). Uses
    // get_edge_of_outer_loop_of_componentBSFace(id) to select that component,
    // then biggest_component_pixel_mask(). Must be called after calculateShells()
    // and before releaseVoronoi(). NOTE: changes the internal selected-component
    // state, so call AFTER extractBiggestComponentMaskFromVoronoi() if both needed.
    std::vector<char> extractComponentMaskById(int component_id);

    // Basic statistics used by StatisticsService
    struct Stats {
        std::string filename;
        int         peelIndex   = 0;
        int         shellCount  = 0;
        double      mu          = 0.0;
        double      sigma       = 0.0;
        bool        isPeeled    = false;
        bool        hasStats    = false;
    };
    Stats collectStats() const;

    // Access raw DCMTK object for saving
    DcmFileFormat* dcmFileFormat() const { return m_fileFormat.get(); }

    // Public: shell statistics for the shell-analysis plot widgets
    std::vector<std::pair<double,double>>
        extractShellStatistics(bool excludeOutermost = EXCLUDE_OUTERMOST_SHELL) const;

private:
    // -------------------------------------------------------------------------
    // DICOM image extraction
    // -------------------------------------------------------------------------
    void extractImageFromDicom();

    // -------------------------------------------------------------------------
    // Shell calculation internals
    // -------------------------------------------------------------------------

    // Shared Voronoi-construction step used by both calculateShells() and
    // computeBiggestComponentMask(): transpose image to column-major, read
    // BitsStored, constructForImageProcessing16bit, set_beta_value,
    // classifyByComponents. Allocates m_voronoi.
    void buildAndClassifyVoronoi();

    void extractOuterShell();
    void calculateInnerShellsAdaptive(bool maximize = false);

    // -------------------------------------------------------------------------
    // Cutting point detection (V2 algorithms)
    //
    // The pure math (covariance inversion, Mahalanobis distance, 3-sigma
    // ellipse test, distance-jump detection) lives in ShellStatistics
    // (models/ShellStatistics.h) as free functions over plain (mu, sigma)
    // values -- these methods just extract the shell statistics, delegate,
    // and store the per-shell bookkeeping (m_shellMahalD/m_shellInside).
    // -------------------------------------------------------------------------
    int calculateCuttingShell(CuttingAlgorithm algo = DEFAULT_CUTTING_ALGORITHM);
    int cuttingEuclideanDistance();
    int cutting3SigmaDistance();
    int cuttingGmmSequential();   // stub → falls back to Euclidean

    // -------------------------------------------------------------------------
    // Data members
    // -------------------------------------------------------------------------
    std::unique_ptr<DcmFileFormat> m_fileFormat;
    std::string  m_filepath;
    double       m_beta;
    int          m_defaultThickness;

    std::vector<uint16_t> m_image;          // processed display image
    std::vector<uint16_t> m_originalImage;  // raw DICOM pixels (for export)
    bool   m_rawDicomReleased = false;      // set true by releaseRawDicom()
    int    m_width  = 0;
    int    m_height = 0;
    double m_imageMax       = 0.0;
    double m_sliceLocation  = 0.0;
    uint16_t m_minIntensity = 0;            // min of original, used during peeling

    std::vector<CTShell> m_shells;
    int  m_peelIndex     = 0;
    int  m_algoPeelIndex = 0;   // algorithm-computed cut, never modified by user actions
    bool m_peeled    = false;
    mutable bool m_doNotChangeThickness = false;    // flag set when shell stats are invalid

    bool m_timedOut = false;

    // Bed removal state (disabled by default — calculateShells() unchanged)
    bool             m_bedRemovalEnabled = false;
    std::string      m_specimenDir;        // cache file root (set by SkinpeelerSession)
    BedDetector::Result m_bedResult;       // per-slice detection result

    // Filled by cutting3SigmaDistance(). Size = m_shells.size().
    std::vector<double> m_shellMahalD;   // shell[0]=-1 (excluded), shell[1+]=actual d
    std::vector<bool>   m_shellInside;   // shell[0]=false, shell[1+]=d<=SIGMA_THRESHOLD

    // Voronoi object — released after shell extraction via releaseVoronoi()
    struct VoronoiImpl;
    std::unique_ptr<VoronoiImpl> m_voronoi;
};

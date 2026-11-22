#pragma once

// =============================================================================
// skinpeeler::Session — the engine's session flow, in plain C++.
//
//   load directory → normalize → per-slice calculate → shell statistics / fit /
//   skin mask → peel → save DICOM
//
// No Qt. The batch command-line program (cli/skinpeeler_cli.cpp) is a thin
// layer over this class: it converts arguments/results and does I/O, nothing
// else.
//
// Two ways to work on a series:
//   * Session object  — all slices loaded and kept (interactive use, peeling).
//   * scan_series() + load_series_slice() — one slice in memory at a time, with
//     the same slice order and the same intensity normalization as a Session
//     on the full directory (batch use on long series).
// =============================================================================

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "config/Settings.h"
#include "models/BedDetector.h"
#include "models/BetaSelection.h"
#include "models/CTImage.h"

namespace skinpeeler {

class Session {
public:
    Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // -------------------------------------------------------------------------
    // Load & normalize (throws std::runtime_error: not a directory / no DICOM)
    // -------------------------------------------------------------------------
    void load_directory(const std::string& path, double beta = DEFAULT_BETA);

    // -------------------------------------------------------------------------
    // Info
    // -------------------------------------------------------------------------
    int slice_count() const {
        return static_cast<int>(m_images.size());
    }
    int width() const {
        return m_images.empty() ? 0 : m_images[0]->width();
    }
    int height() const {
        return m_images.empty() ? 0 : m_images[0]->height();
    }
    double beta() const {
        return m_beta;
    }
    bool is_loaded() const {
        return !m_images.empty();
    }
    const std::string& directory() const {
        return m_directory;
    }

    struct SliceInfo {
        int index = 0;
        std::string filename;
        int width = 0;
        int height = 0;
        double location = 0.0;
        bool has_shells = false;
        int shell_count = 0;
        int peel_index = 0;
        bool is_peeled = false;
        bool timed_out = false;
    };
    SliceInfo slice_info(int idx);

    // Direct access to one slice (throws std::out_of_range).
    CTImage& image(int idx);
    const CTImage& image(int idx) const;

    // Beta — change for all slices (clears computed shells)
    void set_beta_all(double beta);

    // Bed removal — applies to subsequent calculate_slice()/calculate_range()
    void set_bed_removal(bool enabled) {
        m_bed_removal_enabled = enabled;
    }
    bool get_bed_removal() const {
        return m_bed_removal_enabled;
    }
    const BedDetector::Result& bed_result(int idx);

    // -------------------------------------------------------------------------
    // Shell calculation — always maximizes, then releases the Voronoi structure
    // -------------------------------------------------------------------------
    struct CalculateResult {
        int shell_count = 0;
        int peel_index = 0;
        bool timed_out = false;
    };
    CalculateResult calculate_slice(int idx);

    // Batch: calculate [start, end) (clamped) for slices without shells.
    // A slice whose calculation fails is skipped. Returns slices visited.
    int calculate_range(int start, int end);

    // -------------------------------------------------------------------------
    // Shell data queries
    // -------------------------------------------------------------------------
    struct ShellStat {
        int shell_index = 0;
        double mu = 0.0;
        double sigma = 0.0;
        int n_vertices = 0;
        double mahal_d = -1.0;  // -1: shell not scored by the cutting rule
        bool inside = false;    // active cutting rule's inside flag
    };
    std::vector<ShellStat> shell_stats(int idx);

    const CTImage::MahalFit& shell_fit(int idx);

    // Shell vertices in pixel space (out-of-image vertices dropped)
    struct OverlayVertex {
        int col = 0;
        int row = 0;
        double radius = 0.0;
    };
    std::vector<std::vector<OverlayVertex>> shell_overlay(int idx);

    // Flat height*width row-major mask, 1 = skin pixel (shells[0..cut]).
    // cut_override >= 0 replaces the slice's peel index.
    // Throws std::runtime_error if shells are not calculated yet.
    std::vector<uint8_t> segmentation_mask(int idx, int cut_override = -1);

    // Air-excluded skin mask + air mask + threshold (CTImage::noAirMasks).
    // cut_override >= 0 replaces the slice's peel index.
    // Throws std::runtime_error if shells are not calculated yet.
    CTImage::NoAirMasks no_air_masks(int idx, int cut_override = -1);

    // Beta-selection record of slice idx at the session's current beta
    // (input of BetaSelection::select), per-shell quantities included.
    // Throws std::runtime_error if shells are not calculated yet.
    BetaSelection::SliceRecord beta_selection_record(int idx);

    // -------------------------------------------------------------------------
    // Peeling
    // -------------------------------------------------------------------------
    // "UpperHalf" / "LowerHalf" / "LeftHalf" / "RightHalf"; anything else = WholeBody
    static PeelRegion parse_region(const std::string& region);

    void set_peel_index(int idx, int peel_index);

    // Peel every slice at (its cut + increment). A slice whose shell
    // calculation fails is skipped. Returns the number of slices peeled.
    int peel_auto(int increment, PeelRegion region);

    // Peel one slice, respecting its already-set peel index. Returns 1.
    int peel_slice(int idx, PeelRegion region);

    // -------------------------------------------------------------------------
    // Export
    // -------------------------------------------------------------------------
    // Writes every slice as "peeled-<file>" into output_dir; returns files saved.
    int save_dicom(const std::string& output_dir);
    std::string statistics_csv();

    // Release raw DICOM buffers (analysis-only sessions; save_dicom() must not
    // be called afterwards).
    void release_raw_dicom_all();
    void release_raw_dicom(int idx);

    void reset();

    // =========================================================================
    // Streaming access to a series — one slice in memory at a time
    // =========================================================================
    struct SeriesFile {
        std::string path;
        std::uintmax_t size_bytes = 0;
        bool readable = false;
        std::string error;     // why it was skipped (when !readable)
        int slice_index = -1;  // position in slice order; -1 when !readable
        double slice_location = 0.0;
        double image_max = 0.0;  // per-slice maximum before normalization
        int width = 0;
        int height = 0;
    };
    struct SeriesScan {
        std::string directory;
        std::vector<SeriesFile> files;  // every .dcm file, in file-number order
        std::vector<int> order;         // order[slice_index] = index into files
        double global_max = 0.0;        // max image_max over readable files

        int slice_count() const {
            return static_cast<int>(order.size());
        }
        const SeriesFile& slice(int slice_index) const {
            return files.at(order.at(slice_index));
        }
    };
    using WarningCallback = std::function<void(const std::string&)>;

    // First pass over the FULL series: decodes each file once (one at a time,
    // released immediately) to obtain its slice location and intensity maximum.
    // Slice order and readable-file set are exactly those of load_directory().
    // Unreadable files (zero-byte, not DICOM, ...) are recorded with
    // readable == false and reported through on_warning; they get no index.
    // Throws std::runtime_error if `directory` is not a directory.
    static SeriesScan scan_series(const std::string& directory,
                                  const WarningCallback& on_warning = nullptr);

    // Load ONE slice of a scanned series, normalized with scan.global_max the
    // way load_directory() normalizes it — the returned image is pixel-identical
    // to image(slice_index) of a Session loaded on the full directory.
    // Throws std::out_of_range / CTImage::DicomLoadError.
    static std::unique_ptr<CTImage> load_series_slice(const SeriesScan& scan, int slice_index,
                                                      double beta = DEFAULT_BETA);

private:
    CTImage& at(int idx);

    std::vector<std::unique_ptr<CTImage>> m_images;
    std::string m_directory;
    double m_beta = DEFAULT_BETA;
    bool m_bed_removal_enabled = false;
};

}  // namespace skinpeeler

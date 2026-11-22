#include "engine/Session.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <system_error>

#include "services/DicomService.h"
#include "services/StatisticsService.h"
#include "utils/CoordinateTransform.h"

namespace skinpeeler {

namespace fs = std::filesystem;

// =============================================================================
// Load & normalize
// =============================================================================
void Session::load_directory(const std::string& path, double beta) {
    if (!fs::is_directory(path))
        throw std::runtime_error("Not a directory: " + path);

    m_images = DicomService::loadDirectory(path, beta, DEFAULT_SHELL_THICKNESS);
    if (m_images.empty())
        throw std::runtime_error("No DICOM files found in: " + path);

    // NOTE: DicomService::loadDirectory() has already normalized once; this is
    // a second pass with the same global maximum (CTImage::imageMax() is the
    // pre-normalization maximum). load_series_slice() reproduces both passes.
    DicomService::normalizeIntensities(m_images);
    m_beta = beta;
    m_directory = path;
}

// =============================================================================
// Info
// =============================================================================
CTImage& Session::at(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_images.size()))
        throw std::out_of_range("Slice index out of range: " + std::to_string(idx));
    return *m_images[idx];
}

CTImage& Session::image(int idx) {
    return at(idx);
}

const CTImage& Session::image(int idx) const {
    if (idx < 0 || idx >= static_cast<int>(m_images.size()))
        throw std::out_of_range("Slice index out of range: " + std::to_string(idx));
    return *m_images[idx];
}

Session::SliceInfo Session::slice_info(int idx) {
    auto& img = at(idx);
    SliceInfo info;
    info.index = idx;
    info.filename = fs::path(img.filepath()).filename().string();
    info.width = img.width();
    info.height = img.height();
    info.location = img.sliceLocation();
    info.has_shells = img.hasShells();
    info.shell_count = static_cast<int>(img.shells().size());
    info.peel_index = img.peelIndex();
    info.is_peeled = img.isPeeled();
    info.timed_out = img.timedOut();
    return info;
}

void Session::set_beta_all(double beta) {
    m_beta = beta;
    for (auto& img : m_images)
        img->setBeta(beta);
}

const BedDetector::Result& Session::bed_result(int idx) {
    return at(idx).bedResult();
}

// =============================================================================
// Shell calculation
// =============================================================================
Session::CalculateResult Session::calculate_slice(int idx) {
    auto& img = at(idx);
    img.setBedRemoval(m_bed_removal_enabled);
    img.setSpecimenDir(m_directory);
    img.calculateShells();
    img.releaseVoronoi();

    CalculateResult result;
    result.shell_count = static_cast<int>(img.shells().size());
    result.peel_index = img.peelIndex();
    result.timed_out = img.timedOut();
    return result;
}

int Session::calculate_range(int start, int end) {
    start = std::max(0, start);
    end = std::min(end, slice_count());
    int count = 0;
    for (int i = start; i < end; ++i) {
        auto& img = *m_images[i];
        if (!img.hasShells()) {
            img.setBedRemoval(m_bed_removal_enabled);
            img.setSpecimenDir(m_directory);
            try {
                img.calculateShells();
            } catch (...) {
                // skip failed slices
            }
            img.releaseVoronoi();
        }
        ++count;
    }
    return count;
}

// =============================================================================
// Shell data queries
// =============================================================================
std::vector<Session::ShellStat> Session::shell_stats(int idx) {
    auto& img = at(idx);
    const auto& shells = img.shells();
    const auto& mahalD = img.shellMahalD();
    const auto& inside = img.shellInside();

    std::vector<ShellStat> result;
    result.reserve(shells.size());
    for (int i = 0; i < static_cast<int>(shells.size()); ++i) {
        ShellStat st;
        st.shell_index = i;
        st.mu = shells[i].mu();
        st.sigma = shells[i].sigma();
        st.n_vertices = static_cast<int>(shells[i].size());
        st.mahal_d = (i < static_cast<int>(mahalD.size())) ? mahalD[i] : -1.0;
        st.inside = (i < static_cast<int>(inside.size())) ? inside[i] : false;
        result.push_back(st);
    }
    return result;
}

const CTImage::MahalFit& Session::shell_fit(int idx) {
    return at(idx).mahalFit();
}

std::vector<std::vector<Session::OverlayVertex>> Session::shell_overlay(int idx) {
    auto& img = at(idx);
    const int h = img.height(), w = img.width();

    std::vector<std::vector<OverlayVertex>> result;
    result.reserve(img.shells().size());
    for (const auto& shell : img.shells()) {
        std::vector<OverlayVertex> verts;
        verts.reserve(shell.size());
        for (const auto& v : shell) {
            auto [row, col] = CoordinateTransform::voronoiToImagePixel(v.x, v.y, h, w);
            if (CoordinateTransform::isValidPixel(row, col, h, w)) {
                OverlayVertex ov;
                ov.col = col;
                ov.row = row;
                ov.radius = v.radius;
                verts.push_back(ov);
            }
        }
        result.push_back(std::move(verts));
    }
    return result;
}

std::vector<uint8_t> Session::segmentation_mask(int idx, int cut_override) {
    auto& img = at(idx);
    if (!img.hasShells())
        throw std::runtime_error("Shells not yet calculated for slice " + std::to_string(idx) +
                                 ". Call calculate_slice() first.");
    return img.skinMask(cut_override);
}

CTImage::NoAirMasks Session::no_air_masks(int idx, int cut_override) {
    auto& img = at(idx);
    if (!img.hasShells())
        throw std::runtime_error("Shells not yet calculated for slice " + std::to_string(idx) +
                                 ". Call calculate_slice() first.");
    return img.noAirMasks(cut_override);
}

BetaSelection::SliceRecord Session::beta_selection_record(int idx) {
    auto& img = at(idx);
    if (!img.hasShells())
        throw std::runtime_error("Shells not yet calculated for slice " + std::to_string(idx) +
                                 ". Call calculate_slice() first.");
    BetaSelection::SliceRecord rec;
    rec.sliceIndex = idx;
    rec.beta = img.beta();
    rec.judgeable = img.judgeable();
    rec.changepointFound = img.mahalFit().valid;
    rec.cut = img.algoPeelIndex();
    rec.b2Contrast = img.b2Contrast();
    rec.nShells = static_cast<int>(img.shells().size());
    const auto& comp = img.componentSummary();
    if (comp.valid) {
        rec.nComponents = comp.nComponents;
        rec.area = comp.area;
        rec.nInnerLoops = comp.nInnerLoops;
    }

    const auto& shells = img.shells();
    const auto& mahalD = img.shellMahalD();
    std::vector<BetaSelection::ShellRow> rows(shells.size());
    for (size_t i = 0; i < shells.size(); ++i) {
        rows[i].shellIndex = static_cast<int>(i);
        if (shells[i].hasStatistics()) {
            rows[i].mu = shells[i].mu();
            rows[i].sigma = shells[i].sigma();
        }
        rows[i].nVertices = static_cast<int>(shells[i].size());
        if (i < mahalD.size() && mahalD[i] >= 0.0)
            rows[i].d2 = mahalD[i] * mahalD[i];
    }
    BetaSelection::fillFromShells(rec, rows, ShellIndex::kOffset + 1);
    return rec;
}

// =============================================================================
// Peeling
// =============================================================================
PeelRegion Session::parse_region(const std::string& region) {
    if (region == "UpperHalf")
        return PeelRegion::UpperHalf;
    if (region == "LowerHalf")
        return PeelRegion::LowerHalf;
    if (region == "LeftHalf")
        return PeelRegion::LeftHalf;
    if (region == "RightHalf")
        return PeelRegion::RightHalf;
    return PeelRegion::WholeBody;
}

void Session::set_peel_index(int idx, int peel_index) {
    at(idx).setPeelIndex(peel_index);
}

int Session::peel_auto(int increment, PeelRegion region) {
    int count = 0;
    for (auto& img : m_images) {
        if (!img->hasShells()) {
            try {
                img->calculateShells();
            } catch (...) {
                continue;
            }
            img->releaseVoronoi();
        }
        img->setPeelIndex(std::max(0, img->peelIndex() + increment));
        img->peelSkin(region);
        ++count;
    }
    return count;
}

int Session::peel_slice(int idx, PeelRegion region) {
    auto& img = at(idx);
    if (!img.hasShells()) {
        img.calculateShells();
        img.releaseVoronoi();
    }
    img.peelSkin(region);
    return 1;
}

// =============================================================================
// Export
// =============================================================================
int Session::save_dicom(const std::string& output_dir) {
    if (m_images.empty())
        return 0;
    // Ensure peeled data is applied
    for (auto& img : m_images) {
        if (img->isPeeled())
            img->applyPeeledToDicom();
    }
    return DicomService::saveDirectory(m_images, output_dir);
}

std::string Session::statistics_csv() {
    auto stats = StatisticsService::collect(m_images);
    return StatisticsService::formatFullReport(stats);
}

void Session::release_raw_dicom_all() {
    for (auto& img : m_images)
        img->releaseRawDicom();
}

void Session::release_raw_dicom(int idx) {
    at(idx).releaseRawDicom();
}

void Session::reset() {
    m_images.clear();
    m_directory.clear();
    m_beta = DEFAULT_BETA;
}

// =============================================================================
// Streaming access
// =============================================================================
Session::SeriesScan Session::scan_series(const std::string& directory,
                                         const WarningCallback& on_warning) {
    if (!fs::is_directory(directory))
        throw std::runtime_error("Not a directory: " + directory);

    SeriesScan scan;
    scan.directory = directory;

    // Same file discovery and pre-sort (trailing file number) as loadDirectory().
    const std::vector<std::string> paths = DicomService::findDicomFiles(directory);
    scan.files.reserve(paths.size());

    std::vector<int> readable;  // indices into scan.files, in file-number order
    for (const auto& path : paths) {
        SeriesFile entry;
        entry.path = path;
        std::error_code ec;
        const std::uintmax_t sz = fs::file_size(path, ec);
        entry.size_bytes = ec ? 0 : sz;

        try {
            // Decoded exactly as loadDirectory() decodes it; released at the
            // end of this iteration (one slice in memory at a time).
            CTImage img = CTImage::fromDicomFile(path, DEFAULT_BETA, DEFAULT_SHELL_THICKNESS);
            entry.readable = true;
            entry.slice_location = img.sliceLocation();
            entry.image_max = img.imageMax();
            entry.width = img.width();
            entry.height = img.height();
        } catch (const std::exception& e) {
            entry.readable = false;
            entry.error = e.what();
            if (on_warning)
                on_warning("skipping unreadable file " + fs::path(path).filename().string() + " (" +
                           std::to_string(entry.size_bytes) + " bytes): " + e.what());
        }

        scan.files.push_back(std::move(entry));
        if (scan.files.back().readable)
            readable.push_back(static_cast<int>(scan.files.size()) - 1);
    }

    // Same ordering as loadDirectory(): stable sort of the readable slices by
    // slice location (ties keep file-number order).
    std::stable_sort(readable.begin(), readable.end(), [&scan](int a, int b) {
        return scan.files[a].slice_location < scan.files[b].slice_location;
    });
    scan.order = std::move(readable);
    for (int i = 0; i < static_cast<int>(scan.order.size()); ++i)
        scan.files[scan.order[i]].slice_index = i;

    // Same global maximum as DicomService::normalizeIntensities().
    double global_max = 0.0;
    for (int fi : scan.order)
        global_max = std::max(global_max, scan.files[fi].image_max);
    scan.global_max = global_max;

    return scan;
}

std::unique_ptr<CTImage> Session::load_series_slice(const SeriesScan& scan, int slice_index,
                                                    double beta) {
    if (slice_index < 0 || slice_index >= scan.slice_count())
        throw std::out_of_range("Slice index out of range: " + std::to_string(slice_index));

    const SeriesFile& entry = scan.slice(slice_index);
    auto img = std::make_unique<CTImage>(
        CTImage::fromDicomFile(entry.path, beta, DEFAULT_SHELL_THICKNESS));

    // load_directory() normalizes every slice twice with the same global
    // maximum (once inside DicomService::loadDirectory(), once more in
    // load_directory() itself). Reproduced pass for pass so the streamed slice
    // is pixel-identical to the session's. (normalizeToMax() maps a
    // non-positive maximum to 1, as normalizeIntensities() does.)
    img->normalizeToMax(scan.global_max);
    img->normalizeToMax(scan.global_max);
    return img;
}

}  // namespace skinpeeler

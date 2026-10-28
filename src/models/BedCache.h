#pragma once

#include <string>
#include "BedDetector.h"

// =============================================================================
// BedCache
//
// Cache for BedDetector::Result, keyed by (specimen_dir, beta, algorithm_version).
//
// Cache path:
//   <specimen_dir>/.skinpeeler/bed_cache_beta<BETA_3DECIMAL>_v<VER>.json
//
// Directory is auto-created with std::filesystem::create_directories().
//
// JSON schema (per-cache-file):
// {
//   "version": 1,
//   "beta": 0.68,
//   "specimen_dir": "/path/to/specimen",
//   "slices": [
//     {
//       "filepath": "filename.dcm",
//       "detected": true,
//       "n_bed_candidates": 2,
//       "bed_component_ids": [3, 5],
//       "new_body0_component_id": 1,
//       "envelope_y": [512, 510, ...]
//     },
//     ...
//   ]
// }
//
// Design:
//   - load() searches the cache file for a matching filepath (basename only).
//   - save() re-reads the existing file, upserts the entry, and rewrites.
//   - Thread safety: not guaranteed — single-threaded use assumed.
//   - No external JSON library — uses a minimal self-contained parser/writer.
// =============================================================================

class BedCache {
public:
    // Cache files are versioned so that any change to BedDetector's detection
    // behavior invalidates stale results instead of silently reusing them.
    // Bump ALGORITHM_VERSION whenever detect() would produce a different
    // result for the same input.
    //
    // Current behavior (version 7):
    //   - body0 is evaluated like every other component in isBedCandidate(),
    //     so it can itself be classified as bed and replaced via
    //     new_body0_component_id.
    //   - extractTopBoundary() scans the full image width per column (column
    //     scan is not bounded by VD-area-derived radius, since VD area is a
    //     Voronoi disk cell area sum, not a pixel count, and underestimates
    //     the bed's true width).
    //   - LowerEnvelope carries both top and bottom boundary y per column
    //     (bottom_y_at_col), letting callers substitute only the
    //     [top_y, bottom_y] range per column instead of [top_y, image_height).
    //   - Params.theta_pos (50.0) and Params.min_thickness_px (0.0, disabled
    //     by default) exclude cloth false positives from bed detection; see
    //     BedDetector.h for the per-field rationale.
    //   - Params.top_clamp_ratio clamps the envelope's top boundary to keep
    //     cloth+bed merged components from reaching into the body region.
    static constexpr int ALGORITHM_VERSION = 7;

    // Returns true if the cache file for (specimen_dir, beta, version) exists.
    static bool exists(const std::string& specimen_dir, double beta, int version);

    // Load the cached Result for a specific slice file.
    // Returns true and fills result_out on hit; returns false on miss or parse error.
    // slice_filepath: full path to the DICOM file (basename is used for matching).
    static bool load(const std::string& specimen_dir, double beta, int version,
                     const std::string& slice_filepath, BedDetector::Result& result_out);

    // Save (upsert) the Result for a specific slice file.
    // Creates the .skinpeeler/ directory and cache file if they do not exist.
    // Existing entries for the same slice_filepath are replaced.
    static void save(const std::string& specimen_dir, double beta, int version,
                     const std::string& slice_filepath, const BedDetector::Result& result);

private:
    static std::string makeCachePath(const std::string& specimen_dir, double beta, int version);
    static std::string makeBasename(const std::string& filepath);
};

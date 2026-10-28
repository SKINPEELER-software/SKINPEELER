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
    // v2: Fixed Voronoi-to-pixel coordinate conversion in BedDetector
    //     (pixel_row = voro_y + height/2, not height/2 - voro_y).
    //     Body0 is now explicitly skipped in isBedCandidate loop.
    // v3: Fixed extractTopBoundary column scan range.
    //     Old: col scan = [cx_px ± sqrt(VD_area/pi)*1.5]  (~71 px for bed)
    //     New: col scan = [0, image_width-1]  (full width).
    //     Root cause: VD area is Voronoi disk cell area sum, not pixel count.
    //     For the CT bed, VD area ≈ 1700 → radius ≈ 24 px, but actual bed
    //     spans 70~80% of image width (≈ 360~410 px), causing 5× underestimate.
    // v4: LowerEnvelope gains bottom_y_at_col (column-wise maximum bottom row).
    //     Python dump script now substitutes only [top_y, bottom_y] per column
    //     instead of [top_y, image_height). Prevents body tissue above/below
    //     the bed from being incorrectly zeroed when cloth/thin structures
    //     cause the envelope to overshoot.
    //     Cache serializes bottom_y_at_col alongside envelope_y.
    // v5: Cloth false-positive exclusion.
    //     Params.theta_pos: 0.0 → 50.0 (bed must be 50+ px below body0 centroid;
    //       cloth lies 5~30 px below body0 and is excluded).
    //     Params.min_thickness_px: new field (default 15.0). Condition (f) requires
    //       avg(bottom_y - top_y) >= 15 px across boundary columns. Cloth is thin
    //       (5~15 px); real CT bed is thick (30+ px). Excludes cloth false positives
    //       in slices 441~445/451 (MM20-0006 shark CT).
    //     DebugRecord.thickness_px: new field; fail_reason "thick_low" added.
    // v6: min_thickness_px 임계값 무력화 + top_y row clamp 신규.
    //     Params.min_thickness_px: 15.0 → 0.0.
    //       BFS row scan 측정 한계로 진짜 bed도 5~6 px로 측정됨.
    //       임계값이 정상 bed (slice 50/100/300)를 thick_low로 탈락시키는 회귀 발생.
    //       default=0으로 무력화 (조건 자체는 유지, 향후 재도입 가능).
    //     Params.top_clamp_ratio: 0.4 신규.
    //       detect() 내 envelope 산출 직후 top_y clamp 적용.
    //       envelope.y_at_col[col] < image_height * 0.4 이면 0.4*height로 강제.
    //       천 + bed 합쳐진 component의 top boundary가 body 영역까지 침범하는
    //       false mask (slice 441/451 body 잘림) 방지.
    // v7: body0 is no longer skipped in the isBedCandidate loop. body0 is now
    //     evaluated like every other component, so body0_is_bed can actually
    //     become true and the documented "replace body0 when it is itself a
    //     bed" path (new_body0_component_id) becomes reachable. Bumped to
    //     invalidate v6 caches, which were produced while that path was dead
    //     code (body0_is_bed was unconditionally false).
    static constexpr int ALGORITHM_VERSION = 7;

    // Returns true if the cache file for (specimen_dir, beta, version) exists.
    static bool exists(const std::string& specimen_dir, double beta, int version);

    // Load the cached Result for a specific slice file.
    // Returns true and fills result_out on hit; returns false on miss or parse error.
    // slice_filepath: full path to the DICOM file (basename is used for matching).
    static bool load(const std::string& specimen_dir, double beta, int version,
                     const std::string& slice_filepath,
                     BedDetector::Result& result_out);

    // Save (upsert) the Result for a specific slice file.
    // Creates the .skinpeeler/ directory and cache file if they do not exist.
    // Existing entries for the same slice_filepath are replaced.
    static void save(const std::string& specimen_dir, double beta, int version,
                     const std::string& slice_filepath,
                     const BedDetector::Result& result);

private:
    static std::string makeCachePath(const std::string& specimen_dir,
                                     double beta, int version);
    static std::string makeBasename(const std::string& filepath);
};

#pragma once

#include <string>

// =============================================================================
// Application Info
// =============================================================================
inline constexpr const char* APP_NAME = "SKINPEELER";
inline constexpr const char* APP_VERSION = "2.0";
inline constexpr const char* APP_ORGANIZATION =
    "Voronoi Diagram Research Center, Hanyang University, Korea";

// =============================================================================
// Voronoi Algorithm Parameters
// =============================================================================
// DEFAULT_BETA = 0.7: chosen within the BODY regime (safe range 0.67-0.70)
// based on specimen-specific sensitivity sweep (Ch.6.5).
// Values < 0.66 fall in the BED regime where Pass 1 body extraction fails.
inline constexpr double DEFAULT_BETA = 0.7;
inline constexpr int DEFAULT_SHELL_THICKNESS = 10;

inline constexpr int ADAPTIVE_SHELL_MIN = 10;
inline constexpr int ADAPTIVE_SHELL_MAX = 2000;
inline constexpr int ADAPTIVE_SHELL_INCREMENT = 1;
inline constexpr int ADAPTIVE_CHECK_INTERVAL = 5;
inline constexpr int ADAPTIVE_STOPPING_CONSECUTIVE = 3;
inline constexpr double ADAPTIVE_TIME_LIMIT_SECONDS = 120.0;  // hard time limit for shell calc

inline constexpr bool DARKER_PIXEL_BIGGER_DISK = true;

// =============================================================================
// Image Processing
// =============================================================================
inline constexpr int IMAGE_BIT_DEPTH = 16;
inline constexpr uint32_t IMAGE_MAX_VALUE = 65535;

// =============================================================================
// Cutting Shell Detection
// =============================================================================
enum class CuttingAlgorithm {
    THREE_SIGMA,  // 3-sigma ellipse + inter-shell distance (default)
    GMM,          // Gaussian mixture model (not yet implemented)
    EUCLIDEAN     // Legacy: max Euclidean distance between (mu, sigma) points
};

inline constexpr CuttingAlgorithm DEFAULT_CUTTING_ALGORITHM = CuttingAlgorithm::THREE_SIGMA;
inline constexpr bool EXCLUDE_OUTERMOST_SHELL = true;
inline constexpr double SIGMA_THRESHOLD = 3.0;
inline constexpr double DISTANCE_JUMP_SIGMA = 2.0;

// =============================================================================
// Peeling Modes
// =============================================================================
enum class PeelingMode {
    AUTO,
    UNIFORM,
    CUSTOM  // reserved
};

// =============================================================================
// Peeling Region  (which part of the cross-section to peel)
// =============================================================================
enum class PeelRegion { WholeBody, UpperHalf, LowerHalf, LeftHalf, RightHalf };

// =============================================================================
// UI Skin Thickness Limit
// Adjust this value if the maximum detectable skin shell count needs to change.
// =============================================================================
inline constexpr int MAX_SKIN_THICKNESS = 20;
inline constexpr int DEFAULT_INTERVAL_SECTIONS = 5;

// =============================================================================
// UI Settings
// =============================================================================
inline constexpr int DEFAULT_WINDOW_WIDTH = 1400;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 940;
inline constexpr double ZOOM_IN_FACTOR = 1.25;
inline constexpr double ZOOM_OUT_FACTOR = 1.0 / ZOOM_IN_FACTOR;

inline constexpr const char* SHELL_OVERLAY_COLOR = "yellow";
inline constexpr double SHELL_OVERLAY_OPACITY = 1.0;

// =============================================================================
// File I/O
// =============================================================================
inline constexpr const char* DICOM_EXTENSION = ".dcm";
inline constexpr const char* STATISTICS_EXT = ".txt";
inline constexpr const char* PEELED_FILE_PREFIX = "peeled-";

// =============================================================================
// Coordinate Transformation
// =============================================================================
inline constexpr double COORDINATE_OFFSET = 0.5;

// =============================================================================
// Error Messages
// =============================================================================
namespace ErrorMessages {
inline constexpr const char* INVALID_DICOM = "Invalid DICOM file";
inline constexpr const char* MISSING_TRANSFER_SYNTAX = "Missing TransferSyntaxUID";
inline constexpr const char* FILE_READ_ERROR = "Failed to read file";
inline constexpr const char* FILE_WRITE_ERROR = "Failed to write file";
inline constexpr const char* NO_FILES_LOADED = "No DICOM files loaded";
inline constexpr const char* SHELL_CALC_ERROR = "Failed to calculate shells";
}  // namespace ErrorMessages

// =============================================================================
// Shell Index Conversion
// m_shells index space <-> 1-based display label (s1, s2, ...)
// statsOffset = 1 when EXCLUDE_OUTERMOST_SHELL=true (outermost excluded from stats)
// =============================================================================
namespace ShellIndex {
inline constexpr int kOffset = EXCLUDE_OUTERMOST_SHELL ? 1 : 0;

// m_shells index -> 1-based display label (e.g. shells[2] -> s2 when offset=1)
inline int toLabel(int shellsIdx) {
    return shellsIdx - kOffset + 1;
}

// 1-based display label -> m_shells index
inline int toShellsIdx(int label) {
    return label + kOffset - 1;
}
}  // namespace ShellIndex

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include "models/CTImage.h"

// =============================================================================
// StatisticsService
// Collects per-slice statistics and formats text reports.
// =============================================================================
class StatisticsService {
public:
    using StatsList = std::vector<CTImage::Stats>;

    // -------------------------------------------------------------------------
    // Inter-shell distance statistics
    // shellStats: vector of (mu, sigma) pairs per shell (stats-index space)
    // -------------------------------------------------------------------------
    struct InterShellStats {
        std::vector<double> distances;  // consecutive euclidean distances
        double mean = 0.0;
        double max  = 0.0;
        double min  = 0.0;
        bool   valid = false;
    };

    static StatsList collect(const std::vector<std::unique_ptr<CTImage>>& images);

    // Text formatting (mirrors Python set_txt_statistics / format_* methods)
    static std::string formatHeader();
    static std::string formatTable(const StatsList& stats);
    static std::string formatSummary(const StatsList& stats);
    static std::string formatFullReport(const StatsList& stats);

    // Save to file
    static bool saveToFile(const std::string& path, const StatsList& stats);

    // Compute Euclidean distances between consecutive (mu, sigma) shell points
    static InterShellStats computeInterShellDistances(
        const std::vector<std::pair<double,double>>& shellStats);
};

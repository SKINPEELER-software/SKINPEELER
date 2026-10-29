#include "StatisticsService.h"
#include "config/Settings.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cmath>

#include <sstream>
#include <iomanip>
#include <numeric>

StatisticsService::StatsList StatisticsService::collect(
    const std::vector<std::unique_ptr<CTImage>>& images) {
    StatsList list;
    list.reserve(images.size());
    for (const auto& img : images)
        list.push_back(img->collectStats());
    return list;
}

std::string StatisticsService::formatHeader() {
    std::ostringstream oss;
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local{};
#ifdef _WIN32
    localtime_s(&tm_local, &t);
#else
    localtime_r(&t, &tm_local);
#endif
    char datebuf[32];
    std::strftime(datebuf, sizeof(datebuf), "%b %d, %Y", &tm_local);

    oss << "Created by " << APP_NAME << " " << APP_VERSION << "  " << datebuf << "\n"
        << APP_ORGANIZATION << "\n\n";
    return oss.str();
}

std::string StatisticsService::formatTable(const StatsList& stats) {
    std::ostringstream oss;
    oss << std::left << std::setw(6) << "Index" << std::setw(30) << "Filename" << std::setw(12)
        << "Thickness" << std::setw(12) << "mu" << std::setw(12) << "sigma"
        << "\n";
    oss << std::string(72, '-') << "\n";

    for (int i = 0; i < static_cast<int>(stats.size()); ++i) {
        const auto& s = stats[i];
        std::string fname = std::filesystem::path(s.filename).filename().string();
        oss << std::left << std::setw(6) << (i + 1) << std::setw(30) << fname.substr(0, 29)
            << std::setw(12) << s.peelIndex;
        if (s.hasStats) {
            oss << std::fixed << std::setprecision(4) << std::setw(12) << s.mu << std::setw(12)
                << s.sigma;
        } else {
            oss << std::setw(12) << "N/A" << std::setw(12) << "N/A";
        }
        oss << "\n";
    }
    return oss.str();
}

std::string StatisticsService::formatSummary(const StatsList& stats) {
    if (stats.empty())
        return "";

    int peeled = 0;
    double totalMu = 0, totalSigma = 0;
    int validCount = 0;

    for (const auto& s : stats) {
        if (s.isPeeled)
            ++peeled;
        if (s.hasStats) {
            totalMu += s.mu;
            totalSigma += s.sigma;
            ++validCount;
        }
    }

    std::ostringstream oss;
    oss << "\n--- Summary ---\n"
        << "Total images : " << stats.size() << "\n"
        << "Peeled       : " << peeled << "\n";
    if (validCount > 0) {
        oss << std::fixed << std::setprecision(4) << "Avg mu       : " << totalMu / validCount
            << "\n"
            << "Avg sigma    : " << totalSigma / validCount << "\n";
    }
    oss << "\nEOF\n";
    return oss.str();
}

std::string StatisticsService::formatFullReport(const StatsList& stats) {
    return formatHeader() + formatTable(stats) + formatSummary(stats);
}

bool StatisticsService::saveToFile(const std::string& path, const StatsList& stats) {
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << formatFullReport(stats);
    return file.good();
}

// =============================================================================
// computeInterShellDistances
// =============================================================================
StatisticsService::InterShellStats StatisticsService::computeInterShellDistances(
    const std::vector<std::pair<double, double>>& shellStats) {
    InterShellStats result;
    if (shellStats.size() < 2)
        return result;

    result.distances.reserve(shellStats.size() - 1);
    for (int i = 0; i + 1 < static_cast<int>(shellStats.size()); ++i) {
        double dmu = shellStats[i + 1].first - shellStats[i].first;
        double dsg = shellStats[i + 1].second - shellStats[i].second;
        result.distances.push_back(std::sqrt(dmu * dmu + dsg * dsg));
    }

    if (result.distances.empty())
        return result;

    result.mean = 0.0;
    result.max = result.distances[0];
    result.min = result.distances[0];
    for (double d : result.distances) {
        result.mean += d;
        result.max = std::max(result.max, d);
        result.min = std::min(result.min, d);
    }
    result.mean /= static_cast<double>(result.distances.size());
    result.valid = true;
    return result;
}

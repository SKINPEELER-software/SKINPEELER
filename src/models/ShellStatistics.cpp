#include "ShellStatistics.h"

#include <cmath>

namespace ShellStatistics {

bool invert2x2(const double cov[2][2], double inv[2][2]) {
    double det = cov[0][0] * cov[1][1] - cov[0][1] * cov[1][0];
    if (std::abs(det) < 1e-10)
        return false;
    inv[0][0] = cov[1][1] / det;
    inv[0][1] = -cov[0][1] / det;
    inv[1][0] = -cov[1][0] / det;
    inv[1][1] = cov[0][0] / det;
    return true;
}

double mahalanobis2D(const double diff[2], const double inv[2][2]) {
    double t0 = inv[0][0] * diff[0] + inv[0][1] * diff[1];
    double t1 = inv[1][0] * diff[0] + inv[1][1] * diff[1];
    return std::sqrt(diff[0] * t0 + diff[1] * t1);
}

int cuttingEuclideanDistance(const std::vector<std::pair<double, double>>& points, int offset) {
    if (points.empty())
        return 0;

    double maxDist = -1.0;
    int maxIndex = 0;

    for (int i = 0; i + 1 < static_cast<int>(points.size()); ++i) {
        double dmu = points[i + 1].first - points[i].first;
        double dsigma = points[i + 1].second - points[i].second;
        double dist = std::sqrt(dmu * dmu + dsigma * dsigma);
        if (dist > maxDist) {
            maxDist = dist;
            maxIndex = i;
        }
    }

    return maxIndex + offset;
}

int findDistanceJump(const std::vector<std::pair<double, double>>& points, int offset,
                     double distanceJumpSigma) {
    if (points.size() < 2)
        return 0;

    std::vector<double> dists;
    for (int i = 0; i + 1 < static_cast<int>(points.size()); ++i) {
        double dmu = points[i + 1].first - points[i].first;
        double ds = points[i + 1].second - points[i].second;
        dists.push_back(std::sqrt(dmu * dmu + ds * ds));
    }

    double dmean = 0;
    for (double d : dists)
        dmean += d;
    dmean /= static_cast<double>(dists.size());

    double dvar = 0;
    for (double d : dists)
        dvar += (d - dmean) * (d - dmean);
    double dstd = std::sqrt(dvar / static_cast<double>(dists.size()));

    double threshold = dmean + distanceJumpSigma * dstd;

    for (int i = 0; i < static_cast<int>(dists.size()); ++i) {
        if (dists[i] > threshold)
            return i + offset;
    }
    return offset;
}

SigmaCuttingResult cutting3SigmaDistance(const std::vector<std::pair<double, double>>& points,
                                         int offset, double sigmaThreshold,
                                         double distanceJumpSigma) {
    SigmaCuttingResult result;

    if (static_cast<int>(points.size()) < 4)
        return result;  // computed == false

    const double np = static_cast<double>(points.size());
    double meanMu = 0, meanSigma = 0;
    for (auto& [mu, sigma] : points) {
        meanMu += mu;
        meanSigma += sigma;
    }
    meanMu /= np;
    meanSigma /= np;

    // Sample covariance (N-1) to match np.cov
    double c00 = 0, c01 = 0, c11 = 0;
    for (auto& [mu, sigma] : points) {
        double dm = mu - meanMu, ds = sigma - meanSigma;
        c00 += dm * dm;
        c01 += dm * ds;
        c11 += ds * ds;
    }
    double div = np - 1.0;
    double cov[2][2] = {{c00 / div, c01 / div}, {c01 / div, c11 / div}};
    double inv[2][2];

    if (!invert2x2(cov, inv))
        return result;  // computed == false

    result.computed = true;
    result.mahalDistances.resize(points.size());
    result.inside.resize(points.size());

    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        double diff[2] = {points[i].first - meanMu, points[i].second - meanSigma};
        double d = mahalanobis2D(diff, inv);
        result.mahalDistances[i] = d;
        result.inside[i] = (d <= sigmaThreshold);
    }

    for (int i = 0; i < static_cast<int>(result.inside.size()); ++i) {
        if (result.inside[i]) {
            result.cuttingIndex =
                (i == 0) ? findDistanceJump(points, offset, distanceJumpSigma) : (i - 1) + offset;
            return result;
        }
    }
    // All outside -> distance jump
    result.cuttingIndex = findDistanceJump(points, offset, distanceJumpSigma);
    return result;
}

}  // namespace ShellStatistics

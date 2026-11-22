#include "ShellStatistics.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ShellStatistics {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

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

void invert2x2OrPinv(const double cov[2][2], double inv[2][2]) {
    const double a = cov[0][0], b = cov[0][1], c = cov[1][0], d = cov[1][1];
    const double det = a * d - b * c;
    if (det != 0.0) {
        inv[0][0] = d / det;
        inv[0][1] = -b / det;
        inv[1][0] = -c / det;
        inv[1][1] = a / det;
        return;
    }
    // Singular: pseudo-inverse of the symmetric matrix via its eigen
    // decomposition (eigenvalues below rcond * max are dropped, rcond = 1e-15
    // as in np.linalg.pinv).
    const double off = 0.5 * (b + c);
    const double mean = 0.5 * (a + d);
    const double rad = std::sqrt(0.25 * (a - d) * (a - d) + off * off);
    const double l1 = mean + rad, l2 = mean - rad;
    const double theta = 0.5 * std::atan2(2.0 * off, a - d);
    const double v1[2] = {std::cos(theta), std::sin(theta)};
    const double v2[2] = {-std::sin(theta), std::cos(theta)};
    const double cut = 1e-15 * std::max(std::abs(l1), std::abs(l2));
    const double i1 = (std::abs(l1) > cut) ? 1.0 / l1 : 0.0;
    const double i2 = (std::abs(l2) > cut) ? 1.0 / l2 : 0.0;
    for (int r = 0; r < 2; ++r)
        for (int q = 0; q < 2; ++q)
            inv[r][q] = i1 * v1[r] * v1[q] + i2 * v2[r] * v2[q];
}

double gaussianLogLik(const double* x, int n, double mu, double sigma2) {
    if (n == 0)
        return 0.0;
    if (sigma2 <= 0.0)
        sigma2 = 1e-12;
    double ss = 0.0;
    for (int i = 0; i < n; ++i)
        ss += (x[i] - mu) * (x[i] - mu);
    return -0.5 * n * std::log(2.0 * kPi * sigma2) - 0.5 * ss / sigma2;
}

namespace {
// Mean and population variance (ddof = 0) of x[0..n).
void meanVar(const double* x, int n, double& mean, double& var) {
    double s = 0.0;
    for (int i = 0; i < n; ++i)
        s += x[i];
    mean = s / n;
    double ss = 0.0;
    for (int i = 0; i < n; ++i)
        ss += (x[i] - mean) * (x[i] - mean);
    var = ss / n;
}
}  // namespace

BicTerms bicChangepointTerms(const std::vector<double>& d2) {
    BicTerms terms;
    const int m = static_cast<int>(d2.size());
    if (m < 3)
        return terms;
    terms.defined = true;
    const double* x = d2.data();

    // H0: single Gaussian, p = 2
    double mu0, s20;
    meanVar(x, m, mu0, s20);
    const double lnM = std::log(static_cast<double>(m));
    const double ic0 = -2.0 * gaussianLogLik(x, m, mu0, s20) + 2 * lnM;

    // H1: two-segment heteroscedastic, p = 4
    int bestK = -1;
    double bestIc = std::numeric_limits<double>::infinity();
    for (int k = 1; k < m; ++k) {
        double muO, s2O, muI, s2I;
        meanVar(x, k, muO, s2O);
        meanVar(x + k, m - k, muI, s2I);
        s2O = std::max(s2O, 1e-12);
        s2I = std::max(s2I, 1e-12);
        const double l1 = gaussianLogLik(x, k, muO, s2O) + gaussianLogLik(x + k, m - k, muI, s2I);
        const double ic1 = -2.0 * l1 + 4 * lnM;
        if (ic1 < bestIc) {
            bestIc = ic1;
            bestK = k;
        }
    }
    terms.ic0 = ic0;
    terms.bestIc1 = bestIc;
    terms.bestK = bestK;
    if (bestK < 0 || !std::isfinite(bestIc))
        terms.changepoint = -1;
    else if (ic0 <= bestIc)
        terms.changepoint = -1;
    else
        terms.changepoint = bestK;
    return terms;
}

int bicChangepoint(const std::vector<double>& d2) {
    return bicChangepointTerms(d2).changepoint;
}

int cutFromThreshold(const std::vector<double>& d, int firstShellIndex, double tau) {
    const int n = static_cast<int>(d.size());
    if (n == 0)
        return 1;
    for (int i = 0; i < n; ++i) {
        if (d[i] <= tau)
            return std::max(firstShellIndex + i - 1, 1);
    }
    return firstShellIndex + n - 1;
}

MahalBicResult cuttingMahalBic(const std::vector<std::pair<double, double>>& points, int offset) {
    MahalBicResult result;

    // Fewer than 4 non-air shells -> not computed (harness: too_few_shells).
    if (static_cast<int>(points.size()) < 4)
        return result;

    // Fit on S2..Sm = points[1..]
    const int n = static_cast<int>(points.size()) - 1;
    const double dn = static_cast<double>(n);
    double mMu = 0, mSg = 0;
    for (int i = 1; i <= n; ++i) {
        mMu += points[i].first;
        mSg += points[i].second;
    }
    mMu /= dn;
    mSg /= dn;

    // Sample covariance (N-1) to match np.cov
    double c00 = 0, c01 = 0, c11 = 0;
    for (int i = 1; i <= n; ++i) {
        const double dm = points[i].first - mMu, ds = points[i].second - mSg;
        c00 += dm * dm;
        c01 += dm * ds;
        c11 += ds * ds;
    }
    const double div = dn - 1.0;
    result.meanMu = mMu;
    result.meanSigma = mSg;
    result.cov[0][0] = c00 / div;
    result.cov[0][1] = result.cov[1][0] = c01 / div;
    result.cov[1][1] = c11 / div;
    double inv[2][2];
    invert2x2OrPinv(result.cov, inv);

    const int total = static_cast<int>(points.size());
    result.mahalDistances.resize(total);
    for (int i = 0; i < total; ++i) {
        const double diff[2] = {points[i].first - mMu, points[i].second - mSg};
        double q = diff[0] * (inv[0][0] * diff[0] + inv[0][1] * diff[1]) +
                   diff[1] * (inv[1][0] * diff[0] + inv[1][1] * diff[1]);
        result.mahalDistances[i] = std::sqrt(std::max(q, 0.0));
    }

    // BIC changepoint on d^2 of S2..Sm
    std::vector<double> dFit(result.mahalDistances.begin() + 1, result.mahalDistances.end());
    std::vector<double> d2(dFit.size());
    for (size_t i = 0; i < dFit.size(); ++i)
        d2[i] = dFit[i] * dFit[i];

    result.computed = true;
    result.inside.assign(total, false);
    const int k = bicChangepoint(d2);
    if (k < 0) {
        // No changepoint preferred -> last shell index (harness behaviour).
        result.cuttingIndex = offset + total - 1;
        return result;
    }
    result.changepointFound = true;
    result.changepointPos = k;
    const double c2 = 0.5 * (std::max(d2[k - 1], 0.0) + std::max(d2[k], 0.0));
    result.tau = std::sqrt(std::max(c2, 0.0));
    for (int i = 1; i < total; ++i)
        result.inside[i] = (result.mahalDistances[i] <= result.tau);
    result.cuttingIndex = cutFromThreshold(dFit, offset + 1, result.tau);
    return result;
}

}  // namespace ShellStatistics

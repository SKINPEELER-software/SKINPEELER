#include "CTShell.h"
#include <numeric>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <string>

void CTShell::addVertex(double x, double y, double radius) {
    m_vertices.push_back({x, y, radius});
    // invalidate cached statistics
    m_mu.reset();
    m_sigma.reset();
}

void CTShell::clear() {
    m_vertices.clear();
    m_mu.reset();
    m_sigma.reset();
}

void CTShell::calculateStatistics() {
    if (m_vertices.empty()) {
        m_mu.reset();
        m_sigma.reset();
        return;
    }

    // Compute mu and sigma of diameter (= radius * 2) directly from m_vertices.
    // Since diameter = 2*radius: mean(diameter) = 2*mean(radius), std(diameter) = 2*std(radius).
    const double n = static_cast<double>(m_vertices.size());

    double sumR = 0.0;
    for (const auto& v : m_vertices)
        sumR += v.radius;
    const double meanR = sumR / n;

    double varSum = 0.0;
    for (const auto& v : m_vertices) {
        double diff = v.radius - meanR;
        varSum += diff * diff;
    }

    m_mu = meanR * 2.0;
    m_sigma = std::sqrt(varSum / n) * 2.0;
}

std::string CTShell::repr() const {
    std::ostringstream oss;
    oss << "CTShell(vertices=" << m_vertices.size();
    if (hasStatistics()) {
        oss << std::fixed << std::setprecision(2) << ", mu=" << *m_mu << ", sigma=" << *m_sigma;
    } else {
        oss << ", statistics=not_calculated";
    }
    oss << ")";
    return oss.str();
}

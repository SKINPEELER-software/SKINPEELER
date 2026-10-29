#pragma once

#include <vector>
#include <optional>
#include <cmath>
#include <string>
#include <utility>

// =============================================================================
// VertexData
// Stores extracted Voronoi vertex data as plain values.
// Keeping coordinates/radius here (instead of QuasiVertex) lets us safely
// release the ImageProcessingBC object after shell traversal.
// =============================================================================
struct VertexData {
    double x;       // Voronoi space X (centered)
    double y;       // Voronoi space Y (centered)
    double radius;  // disk radius from Voronoi diagram
};

// =============================================================================
// CTShell
// A single shell (boundary layer) extracted from the Voronoi diagram.
// Shells are numbered 0 (outermost skin surface) inward.
//
// Statistics (mu, sigma) are computed from diameter = radius * 2,
// using population standard deviation (divide by N), matching original impl.
// =============================================================================
class CTShell {
public:
    CTShell() = default;

    // -------------------------------------------------------------------------
    // Vertex management
    // -------------------------------------------------------------------------
    void addVertex(double x, double y, double radius);
    void clear();

    const std::vector<VertexData>& vertices() const {
        return m_vertices;
    }
    std::size_t size() const {
        return m_vertices.size();
    }
    bool isEmpty() const {
        return m_vertices.empty();
    }

    // -------------------------------------------------------------------------
    // Statistics  (population std-dev, divide by N)
    // -------------------------------------------------------------------------
    void calculateStatistics();

    bool hasStatistics() const {
        return m_mu.has_value() && m_sigma.has_value();
    }

    double mu() const {
        return m_mu.value_or(0.0);
    }
    double sigma() const {
        return m_sigma.value_or(0.0);
    }

    std::pair<std::optional<double>, std::optional<double>> statisticsTuple() const {
        return {m_mu, m_sigma};
    }

    std::string repr() const;

    // -------------------------------------------------------------------------
    // Iteration helpers
    // -------------------------------------------------------------------------
    auto begin() const {
        return m_vertices.begin();
    }
    auto end() const {
        return m_vertices.end();
    }

private:
    std::vector<VertexData> m_vertices;
    std::optional<double> m_mu;
    std::optional<double> m_sigma;
};

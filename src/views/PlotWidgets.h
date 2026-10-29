#pragma once
#include <QWidget>
#include <QPainter>
#include <vector>
#include <utility>
#include <cmath>
#include <string>

// =============================================================================
// PS (PlotStyle) — centralised visual constants for all plot widgets
//
// Font hierarchy (matplotlib rcParams / IEEE journal convention):
//   Title = base (100 %) | Axis label = 81 % | Tick = 69 % | Annotation = 63 %
// =============================================================================
namespace PS {
// ─── Fonts (pt) ──────────────────────────────────────────────────────────
inline constexpr int FONT_BASE = 16;    // plot title          (100 %)
inline constexpr int FONT_AXIS = 13;    // x / y axis labels  ( 81 %)
inline constexpr int FONT_TICK = 11;    // tick numbers       ( 69 %)
inline constexpr int FONT_LABEL = 12;   // key in-plot labels  ( 75 %)
inline constexpr int FONT_ANNOT = 10;   // supplementary text  ( 63 %)
inline constexpr int FONT_LEGEND = 11;  // legend entries     ( 69 %)

// ─── Line widths (px) ────────────────────────────────────────────────────
inline constexpr float LW_DATA_PRIMARY = 3.5f;  // curr / primary curve
inline constexpr float LW_DATA_SKIN = 3.0f;     // skin shell curves
inline constexpr float LW_DATA_NONSKIN = 1.5f;  // non-skin dashed
inline constexpr float LW_GRID = 1.0f;          // grid lines
inline constexpr float LW_ELLIPSE = 3.5f;       // sigma ellipses
inline constexpr float LW_HIGHLIGHT = 3.5f;     // highlight circles
inline constexpr float LW_CROSS = 2.0f;         // center cross marker
inline constexpr float LW_BREAK_MARK = 2.0f;    // axis-break marks
inline constexpr float LW_CONN_LINE = 1.5f;     // scatter connection line

// ─── Marker radii (px) ───────────────────────────────────────────────────
inline constexpr double MK_SKIN = 7.0;
inline constexpr double MK_NONSKIN = 4.0;
inline constexpr double MK_HIGHLIGHT = 12.0;

// ─── Plot area margins (px) ──────────────────────────────────────────────
inline constexpr int MT = 72;   // top    (title space)
inline constexpr int MR = 28;   // right
inline constexpr int MB = 100;  // bottom (x label + tick numbers)
inline constexpr int ML = 115;  // left   (y label + tick numbers)

// ─── Legend geometry ─────────────────────────────────────────────────────
inline constexpr int LEG_ROW_H = 22;   // row height
inline constexpr int LEG_LINE_W = 26;  // line sample width

// ─── Widget minimum heights (px) ─────────────────────────────────────────
inline constexpr int MIN_H_BASE = 500;
inline constexpr int MIN_H_PLOT = 540;

}  // namespace PS

// =============================================================================
// PlotWidget — Base class for individual plot panels (QPainter-based)
// =============================================================================
class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget* parent = nullptr);

    // Last auto-computed range (valid after first paint)
    double lastXMin() const {
        return m_xMin;
    }
    double lastXMax() const {
        return m_xMax;
    }
    double lastYMin() const {
        return m_yMin;
    }
    double lastYMax() const {
        return m_yMax;
    }

public slots:
    void setXRange(double xMin, double xMax);
    void setYRange(double yMin, double yMax);
    void resetAxisRange();

protected:
    // Helpers for derived classes
    QRectF plotArea() const;
    void drawAxes(QPainter& p, const QString& xLabel, const QString& yLabel, double xMin,
                  double xMax, double yMin, double yMax,
                  const QColor& bg = QColor(248, 248, 255)) const;
    void drawTitle(QPainter& p, const QString& title, const QString& subtitle = {}) const;

    // Map data coords to widget pixel coords (call after drawAxes establishes extents)
    double toPixX(double val) const;
    double toPixY(double val) const;

    mutable double m_xMin = 0, m_xMax = 1, m_yMin = 0, m_yMax = 1;
    mutable QPainter* m_painter = nullptr;  // valid only during paintEvent

    bool m_hasManualXRange = false;
    bool m_hasManualYRange = false;
    double m_manXMin = 0.0, m_manXMax = 1.0;
    double m_manYMin = 0.0, m_manYMax = 1.0;
};

// =============================================================================
// GaussianPlotWidget — Plot 1: Gaussian Distribution of Pixel Intensity
//
// calcPeelIdx : algorithm-recommended cut (skin/non-skin boundary, 0-based)
// currThickness: user's current spinbox setting (0-based index)
// =============================================================================
class GaussianPlotWidget : public PlotWidget {
    Q_OBJECT
public:
    using ShellStats = std::vector<std::pair<double, double>>;

    // Legacy constructor (currThickness defaults to calcPeelIdx)
    explicit GaussianPlotWidget(const ShellStats& stats, int calcPeelIdx,
                                QWidget* parent = nullptr);

    // Full constructor
    explicit GaussianPlotWidget(const ShellStats& stats, int calcPeelIdx, int currThickness,
                                QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawGaussianCurves(QPainter& p) const;
    void drawLegend(QPainter& p) const;

    ShellStats m_stats;
    int m_peelIndex;  // algorithm cut (calcPeelIdx)
    int m_currThick;  // user's current thickness

    int m_yPercentile = 98;      // default 98%
    double m_xRangeMult = 1.55;  // default 1.55
};

// =============================================================================
// ScatterPlotWidget — Plot 2: Shell-wise Pixel Intensity Profile (mu vs sigma)
//
// calcPeelIdx : algorithm-recommended cut → red highlight circle
// currThickness: user's current setting   → black highlight circle + "curr" label
// =============================================================================
class ScatterPlotWidget : public PlotWidget {
    Q_OBJECT
public:
    using ShellStats = std::vector<std::pair<double, double>>;

    // Legacy constructor
    explicit ScatterPlotWidget(const ShellStats& stats, int calcPeelIdx, QWidget* parent = nullptr);

    // Full constructor
    explicit ScatterPlotWidget(const ShellStats& stats, int calcPeelIdx, int currThickness,
                               QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawScatter(QPainter& p) const;
    bool computeMeanCov(double& meanMu, double& meanSg, double cov[2][2]) const;
    void drawEllipse3Sigma(QPainter& p) const;

    ShellStats m_stats;
    int m_peelIndex;  // algorithm cut
    int m_currThick;  // user's current thickness
};

// =============================================================================
// HistogramWidget — Plot 3: Histogram of Inter-Shell Distance
// =============================================================================
class HistogramWidget : public PlotWidget {
    Q_OBJECT
public:
    using ShellStats = std::vector<std::pair<double, double>>;

    explicit HistogramWidget(const ShellStats& stats, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawHistogram(QPainter& p) const;
    static int sturgesBins(int n);

    ShellStats m_stats;
    std::vector<double> m_dists;  // inter-shell euclidean distances
    double m_binSize = 0.0;
};

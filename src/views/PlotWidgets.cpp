#include "PlotWidgets.h"

#include <QPaintEvent>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>
#include <vector>

// Skin shell color palette: Red -> Blue -> Green -> Orange -> Purple -> ...
// index 0 = outermost skin (s1)
static QColor skinPaletteColor(int index) {
    static const QColor palette[] = {
        QColor(220, 50, 50),   // 0: Red      (s1)
        QColor(50, 80, 220),   // 1: Blue     (s2)
        QColor(50, 180, 60),   // 2: Green    (s3)
        QColor(230, 140, 30),  // 3: Orange   (s4)
        QColor(150, 50, 180),  // 4: Purple   (s5)
        QColor(20, 180, 180),  // 5: Teal     (s6)
        QColor(180, 180, 30),  // 6: Yellow-green (s7)
        QColor(220, 80, 150),  // 7: Pink     (s8)
    };
    constexpr int paletteSize = static_cast<int>(std::size(palette));
    return palette[index % paletteSize];
}

// shellColor: skin shell (0..peelIndex) -> palette
//             non-skin shell (>peelIndex) -> dark gray
static QColor shellColor(int index, int /*total*/, int peelIndex) {
    if (index > peelIndex)
        return QColor(80, 80, 80);  // non-skin: dark gray
    return skinPaletteColor(index);
}

// 1-based shell label: index i -> "s(i+1)"
static QString shellLabel(int i) {
    return QString("s%1").arg(i + 1);
}

// =============================================================================
// PlotWidget
// =============================================================================
PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(PS::MIN_H_BASE);
    setAutoFillBackground(false);
}

QRectF PlotWidget::plotArea() const {
    return QRectF(PS::ML, PS::MT, width() - PS::ML - PS::MR, height() - PS::MT - PS::MB);
}

void PlotWidget::drawTitle(QPainter& p, const QString& title, const QString& subtitle) const {
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(PS::FONT_BASE);
    p.setFont(f);
    p.setPen(Qt::black);

    int y = 4;
    p.drawText(QRectF(0, y, width(), 26), Qt::AlignHCenter | Qt::AlignVCenter, title);

    if (!subtitle.isEmpty()) {
        f.setBold(false);
        f.setPointSize(PS::FONT_BASE - 3);
        p.setFont(f);
        p.drawText(QRectF(0, y + 28, width(), 20), Qt::AlignHCenter | Qt::AlignVCenter, subtitle);
    }

    f.setBold(false);
    f.setPointSize(PS::FONT_TICK);
    p.setFont(f);
}

void PlotWidget::drawAxes(QPainter& p, const QString& xLabel, const QString& yLabel, double xMin,
                          double xMax, double yMin, double yMax, const QColor& bg) const {
    m_xMin = xMin;
    m_xMax = xMax;
    m_yMin = yMin;
    m_yMax = yMax;

    QRectF area = plotArea();

    p.fillRect(area, bg);
    p.setPen(QPen(Qt::gray, 1));
    p.drawRect(area);

    QFont f = p.font();
    f.setPointSize(PS::FONT_TICK);
    p.setFont(f);
    p.setPen(Qt::black);

    // X-axis label (bold)
    {
        QFont xf = p.font();
        xf.setBold(true);
        xf.setPointSize(PS::FONT_AXIS);
        p.setFont(xf);
        p.drawText(QRectF(area.left(), area.bottom() + 42, area.width(), 22), Qt::AlignHCenter,
                   xLabel);
        QFont xfr = p.font();
        xfr.setBold(false);
        p.setFont(xfr);
    }

    // Y-axis label (rotated, bold)
    p.save();
    {
        QFont yf = p.font();
        yf.setBold(true);
        yf.setPointSize(PS::FONT_AXIS);
        p.setFont(yf);
        p.translate(area.left() - PS::ML + 14, area.top() + area.height() / 2.0);
        p.rotate(-90);
        p.drawText(QRectF(-80, -12, 160, 24), Qt::AlignHCenter, yLabel);
    }
    p.restore();

    // Grid + tick labels (5 divisions)
    p.setPen(QPen(QColor(210, 210, 210), PS::LW_GRID, Qt::DashLine));
    const int ticks = 4;
    for (int i = 1; i < ticks; ++i) {
        double gx = area.left() + area.width() * i / ticks;
        double gy = area.top() + area.height() * i / ticks;
        p.drawLine(QPointF(gx, area.top()), QPointF(gx, area.bottom()));
        p.drawLine(QPointF(area.left(), gy), QPointF(area.right(), gy));
    }

    p.setPen(Qt::black);
    f.setPointSize(PS::FONT_TICK);
    f.setBold(false);
    p.setFont(f);
    for (int i = 0; i <= ticks; ++i) {
        double xVal = xMin + (xMax - xMin) * i / ticks;
        double yVal = yMax - (yMax - yMin) * i / ticks;
        double px = area.left() + area.width() * i / ticks;
        double py = area.top() + area.height() * i / ticks;

        p.drawText(QRectF(px - 30, area.bottom() + 8, 60, 18), Qt::AlignHCenter,
                   QString::number(xVal, 'f', 2));
        p.drawText(QRectF(area.left() - 62, py - 9, 58, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(yVal, 'f', 3));
    }
}

double PlotWidget::toPixX(double val) const {
    QRectF area = plotArea();
    double range = m_xMax - m_xMin;
    if (range < 1e-12)
        range = 1.0;
    return area.left() + (val - m_xMin) / range * area.width();
}

double PlotWidget::toPixY(double val) const {
    QRectF area = plotArea();
    double range = m_yMax - m_yMin;
    if (range < 1e-12)
        range = 1.0;
    return area.bottom() - (val - m_yMin) / range * area.height();
}

void PlotWidget::setXRange(double xMin, double xMax) {
    m_hasManualXRange = true;
    m_manXMin = xMin;
    m_manXMax = xMax;
    update();
}

void PlotWidget::setYRange(double yMin, double yMax) {
    m_hasManualYRange = true;
    m_manYMin = yMin;
    m_manYMax = yMax;
    update();
}

void PlotWidget::resetAxisRange() {
    m_hasManualXRange = false;
    m_hasManualYRange = false;
    update();
}

// =============================================================================
// GaussianPlotWidget  — Plot 1
// =============================================================================
GaussianPlotWidget::GaussianPlotWidget(const ShellStats& stats, int calcPeelIdx, QWidget* parent)
    : PlotWidget(parent), m_stats(stats), m_peelIndex(calcPeelIdx), m_currThick(calcPeelIdx) {
    setMinimumHeight(PS::MIN_H_PLOT);
}

GaussianPlotWidget::GaussianPlotWidget(const ShellStats& stats, int calcPeelIdx, int currThickness,
                                       QWidget* parent)
    : PlotWidget(parent), m_stats(stats), m_peelIndex(calcPeelIdx), m_currThick(currThickness) {
    setMinimumHeight(PS::MIN_H_PLOT);
}

void GaussianPlotWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    m_painter = &p;

    p.fillRect(rect(), Qt::white);
    drawTitle(p, "Gaussian Distribution of Pixel Intensity");

    if (m_stats.size() < 2) {
        p.drawText(rect(), Qt::AlignCenter, "Insufficient shell data");
        m_painter = nullptr;
        return;
    }

    // ── Peak densities ──
    const int N = static_cast<int>(m_stats.size());
    double globalPeakMax = 0.0;
    std::vector<double> allPeaks;
    allPeaks.reserve(N);
    for (int i = 0; i < N; ++i) {
        double sg = m_stats[i].second;
        if (sg < 1e-9)
            continue;
        double pk = 1.0 / (sg * std::sqrt(2.0 * M_PI));
        globalPeakMax = std::max(globalPeakMax, pk);
        allPeaks.push_back(pk);
    }
    if (globalPeakMax < 1e-9)
        globalPeakMax = 1.0;

    // ── Y range: m_yPercentile-th percentile of all peak densities × 1.1 margin
    double yMax = globalPeakMax * 1.1;  // fallback
    if (!allPeaks.empty()) {
        std::vector<double> sorted = allPeaks;
        std::sort(sorted.begin(), sorted.end());
        int idx = static_cast<int>(sorted.size() * (m_yPercentile / 100.0));
        idx = std::clamp(idx, 0, static_cast<int>(sorted.size()) - 1);
        yMax = sorted[idx] * 1.1;
    }

    // ── X range: full distribution, threshold = 5% of globalPeak, then expand around center
    double threshold = 0.05 * globalPeakMax;
    double xMin_data = std::numeric_limits<double>::max();
    double xMax_data = std::numeric_limits<double>::lowest();
    for (int i = 0; i < N; ++i) {
        double mu = m_stats[i].first;
        double sg = m_stats[i].second;
        if (sg < 1e-9)
            continue;
        double pk = 1.0 / (sg * std::sqrt(2.0 * M_PI));
        if (pk < threshold)
            continue;
        double halfRange = sg * std::sqrt(2.0 * std::log(pk / threshold));
        xMin_data = std::min(xMin_data, mu - halfRange);
        xMax_data = std::max(xMax_data, mu + halfRange);
    }
    if (xMin_data > xMax_data) {
        xMin_data = 0.0;
        xMax_data = 1.0;
    }
    // Expand the current range by m_xRangeMult (center: overall midpoint,
    // biased 30% toward the skin peak)
    {
        double xCenter = (xMin_data + xMax_data) * 0.5;
        double xHalf = (xMax_data - xMin_data) * 0.5;

        // Compute the mean mu of the skin distributions and shift center 30% toward it
        double skinMuSum = 0.0;
        int skinCount = 0;
        for (int i = 0; i <= m_peelIndex && i < N; ++i) {
            if (m_stats[i].second < 1e-9)
                continue;
            skinMuSum += m_stats[i].first;
            ++skinCount;
        }
        if (skinCount > 0) {
            double skinCenter = skinMuSum / skinCount;
            xCenter = xCenter * 0.7 + skinCenter * 0.3;
        }

        xMin_data = xCenter - xHalf * m_xRangeMult;
        xMax_data = xCenter + xHalf * m_xRangeMult;
    }

    // Manual range override
    if (m_hasManualXRange) {
        xMin_data = m_manXMin;
        xMax_data = m_manXMax;
    }
    if (m_hasManualYRange) {
        yMax = m_manYMax;
    }
    double yMin_gauss = m_hasManualYRange ? m_manYMin : 0.0;

    drawAxes(p, "Pixel Intensity", "Density", xMin_data, xMax_data, yMin_gauss, yMax,
             QColor(248, 252, 255));

    drawGaussianCurves(p);
    drawLegend(p);

    m_painter = nullptr;
}

void GaussianPlotWidget::drawGaussianCurves(QPainter& p) const {
    const int N = static_cast<int>(m_stats.size());
    const int steps = 300;
    const double xStep = (m_xMax - m_xMin) / steps;

    // Draw in reverse order so s1 (index 0) is rendered last → on top
    for (int i = N - 1; i >= 0; --i) {
        double mu = m_stats[i].first;
        double sg = m_stats[i].second;
        if (sg < 1e-9)
            continue;

        bool isSkin = (i <= m_peelIndex);
        bool isCurrThick = (i == m_currThick);

        QColor col;
        QPen pen;
        if (isCurrThick) {
            // "curr" shell: shell's own color, primary lineWidth (most prominent)
            col = shellColor(i, N, m_peelIndex);
            pen = QPen(col, PS::LW_DATA_PRIMARY, Qt::SolidLine);
        } else if (isSkin) {
            col = shellColor(i, N, m_peelIndex);
            pen = QPen(col, PS::LW_DATA_SKIN, Qt::SolidLine);
        } else {
            col = shellColor(i, N, m_peelIndex);
            pen = QPen(col, PS::LW_DATA_NONSKIN, Qt::DashLine);
        }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        QPolygonF curve;
        curve.reserve(steps + 1);
        for (int s = 0; s <= steps; ++s) {
            double x = m_xMin + s * xStep;
            double exponent = -(x - mu) * (x - mu) / (2.0 * sg * sg);
            double y = (1.0 / (sg * std::sqrt(2.0 * M_PI))) * std::exp(exponent);
            curve.append(QPointF(toPixX(x), toPixY(y)));
        }
        p.drawPolyline(curve);

        // --- Mode label above peak ---
        // Skin shells (not curr): bold shell label s(i+1)
        // curr shell: bold "curr" label
        if (isSkin || isCurrThick) {
            double peakY = 1.0 / (sg * std::sqrt(2.0 * M_PI));
            double px = toPixX(mu);
            double py = toPixY(peakY);

            QFont lf = p.font();
            lf.setBold(true);
            lf.setPointSize(PS::FONT_LABEL);
            p.setFont(lf);

            // Label: base shell name + asterisk if cut + (curr) if curr
            bool isCut = (i == m_peelIndex);
            QString lbl = shellLabel(i);
            if (isCut)
                lbl += "*";
            if (isCurrThick)
                lbl += "(curr)";

            // Color = distribution's own color (not darker)
            QColor lblColor = shellColor(i, N, m_peelIndex);
            p.setPen(lblColor);
            // Draw label slightly above the peak (wider box for combined labels)
            p.drawText(QRectF(px - 42, py - 26, 84, 22), Qt::AlignHCenter | Qt::AlignVCenter, lbl);

            QFont nf = p.font();
            nf.setBold(false);
            p.setFont(nf);
        }
    }
}

void GaussianPlotWidget::drawLegend(QPainter& p) const {
    const int N = static_cast<int>(m_stats.size());

    // Collect skin shell indices (0..peelIndex)
    // Show all if <= 6, else keep first, every ~K-th, and last
    std::vector<int> skinIdx, nonSkinIdx;
    for (int i = 0; i <= m_peelIndex && i < N; ++i)
        skinIdx.push_back(i);
    for (int i = m_peelIndex + 1; i < N; ++i)
        nonSkinIdx.push_back(i);

    // For skin: show at most 5 representative + peelIndex
    std::vector<int> showSkin;
    if (static_cast<int>(skinIdx.size()) <= 5) {
        showSkin = skinIdx;
    } else {
        showSkin.push_back(skinIdx.front());
        int step = std::max(1, (static_cast<int>(skinIdx.size()) - 1) / 3);
        for (int i = step; i < static_cast<int>(skinIdx.size()) - 1; i += step)
            if (static_cast<int>(showSkin.size()) < 4)
                showSkin.push_back(skinIdx[i]);
        if (showSkin.back() != skinIdx.back())
            showSkin.push_back(skinIdx.back());
        std::sort(showSkin.begin(), showSkin.end());
        showSkin.erase(std::unique(showSkin.begin(), showSkin.end()), showSkin.end());
    }

    // Non-skin shells are represented by a single summary entry "s3~"

    QFont f = p.font();
    f.setPointSize(PS::FONT_LEGEND);
    p.setFont(f);

    int lx = PS::ML + 8;
    int ly = PS::MT + 6;
    const int lh = PS::LEG_ROW_H;
    const int lineW = PS::LEG_LINE_W;

    // --- Skin shells ---
    for (int idx : showSkin) {
        bool isCurr = (idx == m_currThick);
        QColor col = shellColor(idx, N, m_peelIndex);  // always shell color, not black
        float lw = isCurr ? PS::LW_DATA_PRIMARY : PS::LW_DATA_SKIN;
        p.setPen(QPen(col, lw));
        p.drawLine(lx, ly + lh / 2, lx + lineW, ly + lh / 2);
        p.setPen(Qt::black);
        QString lbl = shellLabel(idx);
        if (isCurr)
            lbl += " (curr)";
        QFont lf = p.font();
        lf.setBold(isCurr);
        p.setFont(lf);
        p.drawText(QRectF(lx + lineW + 6, ly, 90, lh), Qt::AlignLeft | Qt::AlignVCenter, lbl);
        lf.setBold(false);
        p.setFont(lf);
        ly += lh;
    }

    // --- Non-skin: single summary entry "sN~" ---
    if (!nonSkinIdx.empty()) {
        p.setPen(QPen(QColor(180, 180, 180), 1, Qt::DashLine));
        p.drawLine(lx, ly + 2, lx + lineW + 96, ly + 2);
        ly += 6;

        QColor nsCol = QColor(30, 30, 30);
        p.setPen(QPen(nsCol, PS::LW_DATA_NONSKIN, Qt::DashLine));
        p.drawLine(lx, ly + lh / 2, lx + lineW, ly + lh / 2);
        p.setPen(Qt::black);
        QFont nf = p.font();
        nf.setPointSize(PS::FONT_LEGEND);
        p.setFont(nf);
        p.drawText(QRectF(lx + lineW + 6, ly, 90, lh), Qt::AlignLeft | Qt::AlignVCenter,
                   shellLabel(nonSkinIdx.front()) + "~");
        ly += lh;
    }
}

// =============================================================================
// ScatterPlotWidget  — Plot 2
// =============================================================================
ScatterPlotWidget::ScatterPlotWidget(const ShellStats& stats, int calcPeelIdx, QWidget* parent)
    : PlotWidget(parent), m_stats(stats), m_peelIndex(calcPeelIdx), m_currThick(calcPeelIdx) {
    setMinimumHeight(PS::MIN_H_PLOT);
}

ScatterPlotWidget::ScatterPlotWidget(const ShellStats& stats, int calcPeelIdx, int currThickness,
                                     QWidget* parent)
    : PlotWidget(parent), m_stats(stats), m_peelIndex(calcPeelIdx), m_currThick(currThickness) {
    setMinimumHeight(PS::MIN_H_PLOT);
}

void ScatterPlotWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    m_painter = &p;

    p.fillRect(rect(), Qt::white);
    drawTitle(p, "Shell-wise Pixel Intensity Profile");

    if (m_stats.size() < 2) {
        p.drawText(rect(), Qt::AlignCenter, "Insufficient shell data");
        m_painter = nullptr;
        return;
    }

    double xMin = m_stats[0].first, xMax = m_stats[0].first;
    double yMin = m_stats[0].second, yMax = m_stats[0].second;
    for (auto& [mu, sg] : m_stats) {
        xMin = std::min(xMin, mu);
        xMax = std::max(xMax, mu);
        yMin = std::min(yMin, sg);
        yMax = std::max(yMax, sg);
    }

    // Expand to include 3-sigma ellipse bounding box
    {
        double mMu, mSg, cov[2][2];
        if (computeMeanCov(mMu, mSg, cov)) {
            double c00 = cov[0][0], c01 = cov[0][1], c11 = cov[1][1];
            if (c00 >= 1e-12) {
                double L11 = std::sqrt(c00);
                double L21 = c01 / L11;
                double L22_sq = c11 - L21 * L21;
                if (L22_sq >= 1e-12) {
                    double L22 = std::sqrt(L22_sq);
                    double eX = 1.5 * L11;
                    double eY = 1.5 * std::sqrt(L21 * L21 + L22 * L22);
                    xMin = std::min(xMin, mMu - eX);
                    xMax = std::max(xMax, mMu + eX);
                    yMin = std::min(yMin, mSg - eY);
                    yMax = std::max(yMax, mSg + eY);
                }
            }
        }
    }

    const double pad = 0.12;
    double rx = xMax - xMin;
    if (rx < 1e-9)
        rx = 1.0;
    double ry = yMax - yMin;
    if (ry < 1e-9)
        ry = 1.0;
    xMin -= rx * pad;
    xMax += rx * pad;
    yMin -= ry * pad;
    yMax += ry * pad;

    // Manual range override
    if (m_hasManualXRange) {
        xMin = m_manXMin;
        xMax = m_manXMax;
    }
    if (m_hasManualYRange) {
        yMin = m_manYMin;
        yMax = m_manYMax;
    }

    drawAxes(p, "Mean (mu)", "Std Dev (sigma)", xMin, xMax, yMin, yMax, QColor(248, 252, 248));

    drawEllipse3Sigma(p);  // draw first so ellipses appear behind scatter points
    drawScatter(p);

    m_painter = nullptr;
}

void ScatterPlotWidget::drawScatter(QPainter& p) const {
    const int N = static_cast<int>(m_stats.size());

    // Connection line
    QPolygonF line;
    line.reserve(N);
    for (int i = 0; i < N; ++i)
        line.append(QPointF(toPixX(m_stats[i].first), toPixY(m_stats[i].second)));

    p.setPen(QPen(QColor(180, 180, 180), PS::LW_CONN_LINE, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(line);

    // Points: skin shells larger (MK_SKIN), non-skin smaller (MK_NONSKIN)
    // Labels: shown only up to cut+1, placed upper-right
    // Label text: base "sN" + "*" if cut + "(curr)" if curr
    for (int i = 0; i < N; ++i) {
        double px = toPixX(m_stats[i].first);
        double py = toPixY(m_stats[i].second);

        bool isSkin = (i <= m_peelIndex);
        bool isCut = (i == m_peelIndex);
        bool isCurr = (i == m_currThick);

        QColor col = shellColor(i, N, m_peelIndex);
        double radius = isSkin ? PS::MK_SKIN : PS::MK_NONSKIN;
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPointF(px, py), radius, radius);

        // Labels only up to cut+1
        if (i <= m_peelIndex + 1) {
            QString lbl = shellLabel(i);
            if (isCut)
                lbl += "*";
            if (isCurr)
                lbl += "(curr)";

            p.setPen(col);  // match label color to point color
            QFont f = p.font();
            f.setPointSize(PS::FONT_LABEL);
            f.setBold(isSkin);
            p.setFont(f);
            p.drawText(QRectF(px + 6, py - 26, 96, 22), lbl);
            f.setBold(false);
            p.setFont(f);
        }
    }

    // --- calcPeelIdx highlight: red outer circle ---
    if (m_peelIndex >= 0 && m_peelIndex < N) {
        double px = toPixX(m_stats[m_peelIndex].first);
        double py = toPixY(m_stats[m_peelIndex].second);
        p.setPen(QPen(QColor(220, 50, 50), PS::LW_HIGHLIGHT));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(px, py), PS::MK_HIGHLIGHT, PS::MK_HIGHLIGHT);
    }

    // --- currThickness highlight: black outer circle (only if curr != cut) ---
    if (m_currThick >= 0 && m_currThick < N && m_currThick != m_peelIndex) {
        double px = toPixX(m_stats[m_currThick].first);
        double py = toPixY(m_stats[m_currThick].second);
        p.setPen(QPen(Qt::black, PS::LW_HIGHLIGHT));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(px, py), PS::MK_HIGHLIGHT, PS::MK_HIGHLIGHT);
    }
}

bool ScatterPlotWidget::computeMeanCov(double& meanMu, double& meanSg, double cov[2][2]) const {
    const int n = static_cast<int>(m_stats.size());
    if (n < 3)
        return false;
    meanMu = meanSg = 0;
    for (auto& [mu, sg] : m_stats) {
        meanMu += mu;
        meanSg += sg;
    }
    meanMu /= n;
    meanSg /= n;

    double c00 = 0, c01 = 0, c11 = 0;
    for (auto& [mu, sg] : m_stats) {
        double dm = mu - meanMu, ds = sg - meanSg;
        c00 += dm * dm;
        c01 += dm * ds;
        c11 += ds * ds;
    }
    double div = static_cast<double>(n - 1);
    cov[0][0] = c00 / div;
    cov[0][1] = cov[1][0] = c01 / div;
    cov[1][1] = c11 / div;
    return true;
}

void ScatterPlotWidget::drawEllipse3Sigma(QPainter& p) const {
    double meanMu, meanSg, cov[2][2];
    if (!computeMeanCov(meanMu, meanSg, cov))
        return;

    double c00 = cov[0][0], c01 = cov[0][1], c11 = cov[1][1];
    if (c00 < 1e-12)
        return;
    double L11 = std::sqrt(c00);
    double L21 = c01 / L11;
    double L22_sq = c11 - L21 * L21;
    if (L22_sq < 1e-12)
        return;
    double L22 = std::sqrt(L22_sq);

    // Sky-blue color for all sigma ellipses
    const QColor ellipseColor(100, 180, 220);
    const float ellipseWidth = PS::LW_ELLIPSE;

    const int sigmaLevels[] = {1, 2, 3};

    const int N_POINTS = 200;
    for (int sigma : sigmaLevels) {
        QPolygonF poly;
        poly.reserve(N_POINTS + 1);

        // Also find the upper-left tangent point for label placement
        // "upper-left" in screen = minimize (px + py)
        double minSum = std::numeric_limits<double>::max();
        double labelPx = 0.0, labelPy = 0.0;

        for (int i = 0; i <= N_POINTS; ++i) {
            double t = 2.0 * M_PI * i / N_POINTS;
            double ct = std::cos(t), st = std::sin(t);
            double dx = sigma * (L11 * ct);
            double dy = sigma * (L21 * ct + L22 * st);
            double px = toPixX(meanMu + dx);
            double py = toPixY(meanSg + dy);
            poly.append(QPointF(px, py));

            double sum = px + py;
            if (sum < minSum) {
                minSum = sum;
                labelPx = px;
                labelPy = py;
            }
        }

        p.setPen(QPen(ellipseColor, ellipseWidth));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(poly);

        // Draw sigma label at the upper-left tangent point
        QFont f = p.font();
        f.setPointSize(PS::FONT_LABEL);
        f.setBold(true);
        p.setFont(f);
        p.setPen(ellipseColor.darker(130));
        QString lbl = QString("%1\u03c3").arg(sigma);
        // Place label just outside the ellipse (offset slightly upper-left)
        p.drawText(QRectF(labelPx - 52, labelPy - 22, 50, 20), Qt::AlignRight | Qt::AlignVCenter,
                   lbl);
        f.setBold(false);
        p.setFont(f);
    }

    // Center cross marker
    double pcx = toPixX(meanMu);
    double pcy = toPixY(meanSg);
    p.setPen(QPen(Qt::black, PS::LW_CROSS));
    p.drawLine(QPointF(pcx - 8, pcy), QPointF(pcx + 8, pcy));
    p.drawLine(QPointF(pcx, pcy - 8), QPointF(pcx, pcy + 8));
}

// =============================================================================
// HistogramWidget  — Plot 3
// =============================================================================
HistogramWidget::HistogramWidget(const ShellStats& stats, QWidget* parent)
    : PlotWidget(parent), m_stats(stats) {
    setMinimumHeight(PS::MIN_H_PLOT);

    const int n = static_cast<int>(stats.size());
    if (n < 2)
        return;
    m_dists.reserve(n - 1);
    for (int i = 0; i + 1 < n; ++i) {
        double dmu = stats[i + 1].first - stats[i].first;
        double dsg = stats[i + 1].second - stats[i].second;
        m_dists.push_back(std::sqrt(dmu * dmu + dsg * dsg));
    }

    int bins = sturgesBins(static_cast<int>(m_dists.size()));
    if (bins < 1)
        bins = 1;
    double maxD = *std::max_element(m_dists.begin(), m_dists.end());
    double minD = *std::min_element(m_dists.begin(), m_dists.end());
    double range = maxD - minD;
    m_binSize = (range < 1e-12) ? 1.0 : range / bins;
}

int HistogramWidget::sturgesBins(int n) {
    if (n <= 1)
        return 1;
    return static_cast<int>(std::ceil(std::log2(n) + 1.0));
}

void HistogramWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    m_painter = &p;

    p.fillRect(rect(), Qt::white);

    QString subtitle;
    if (m_binSize > 0)
        subtitle = QString("Bin size = %1").arg(m_binSize, 0, 'f', 4);
    drawTitle(p, "Histogram of Inter-Shell Distance", subtitle);

    if (m_dists.size() < 2) {
        p.drawText(rect(), Qt::AlignCenter, "Insufficient distance data");
        m_painter = nullptr;
        return;
    }

    drawHistogram(p);
    m_painter = nullptr;
}

void HistogramWidget::drawHistogram(QPainter& p) const {
    if (m_dists.empty() || m_binSize < 1e-12)
        return;

    double minD = *std::min_element(m_dists.begin(), m_dists.end());
    double maxD = *std::max_element(m_dists.begin(), m_dists.end());

    int bins = sturgesBins(static_cast<int>(m_dists.size()));
    if (bins < 1)
        bins = 1;

    std::vector<int> counts(bins, 0);
    for (double d : m_dists) {
        int bin = static_cast<int>((d - minD) / m_binSize);
        bin = std::clamp(bin, 0, bins - 1);
        counts[bin]++;
    }

    int maxCount = *std::max_element(counts.begin(), counts.end());
    if (maxCount == 0)
        maxCount = 1;

    double xPad = m_binSize * 0.5;
    double xMin = minD - xPad;
    double xMax = maxD + m_binSize + xPad;
    double yMax = static_cast<double>(maxCount) * 1.15;

    drawAxes(p, "Shell distance", "Count", xMin, xMax, 0.0, yMax, QColor(252, 252, 248));

    QRectF area = plotArea();

    const QColor barCol = QColor(100, 140, 210);  // medium blue
    for (int i = 0; i < bins; ++i) {
        double bLeft = minD + i * m_binSize;
        double bRight = bLeft + m_binSize;
        double px1 = toPixX(bLeft);
        double px2 = toPixX(bRight);
        double py1 = toPixY(static_cast<double>(counts[i]));
        double py2 = area.bottom();

        double gap = (px2 - px1) * 0.1;
        p.setPen(QPen(barCol.darker(130), 1));
        p.setBrush(barCol);
        p.drawRect(QRectF(px1 + gap, py1, (px2 - px1) - 2 * gap, py2 - py1));

        if (counts[i] > 0) {
            p.setPen(Qt::black);
            QFont f = p.font();
            f.setPointSize(PS::FONT_ANNOT);
            p.setFont(f);
            p.drawText(QRectF(px1, py1 - 18, px2 - px1, 18), Qt::AlignHCenter,
                       QString::number(counts[i]));
        }
    }

    // Summary stats
    double mean = 0, dmax = 0, dmin = std::numeric_limits<double>::max();
    for (double d : m_dists) {
        mean += d;
        dmax = std::max(dmax, d);
        dmin = std::min(dmin, d);
    }
    mean /= static_cast<double>(m_dists.size());

    QFont f = p.font();
    f.setPointSize(PS::FONT_ANNOT);
    p.setFont(f);
    p.setPen(QColor(80, 80, 80));
    QString summary = QString("mean=%1  max=%2  min=%3")
                          .arg(mean, 0, 'f', 4)
                          .arg(dmax, 0, 'f', 4)
                          .arg(dmin, 0, 'f', 4);
    // Place summary inside plot area (top-right)
    p.drawText(QRectF(area.right() - 305, area.top() + 8, 300, 18),
               Qt::AlignRight | Qt::AlignVCenter, summary);
}

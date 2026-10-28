#include "SkinPeelerScene.h"
#include "models/CTImage.h"
#include "models/CTShell.h"
#include "utils/CoordinateTransform.h"
#include "config/Settings.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QImage>
#include <QPixmap>
#include <QPen>
#include <QPainter>
#include <QBrush>
#include <Qt>
#include <QScrollBar>
#include <QMouseEvent>
#include <cmath>

// =============================================================================
// SkinPeelerView
// =============================================================================
SkinPeelerView::SkinPeelerView(QWidget* parent) : QGraphicsView(parent) {
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag);
}

void SkinPeelerView::wheelEvent(QWheelEvent* event) {
    constexpr double ZOOM_FACTOR = 1.15;
    double factor = (event->angleDelta().y() > 0) ? ZOOM_FACTOR : (1.0 / ZOOM_FACTOR);
    scale(factor, factor);
    event->accept();
}

void SkinPeelerView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_annotationMode) {
            // Single pixel toggle on click only (no drag painting)
            QPointF sp = mapToScene(event->pos());
            emit annotationPaint(sp);
            event->accept();
            return;
        } else {
            m_panning = true;
            m_lastPanPoint = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
        event->accept();
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void SkinPeelerView::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void SkinPeelerView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        setCursor(m_annotationMode ? Qt::CrossCursor : Qt::ArrowCursor);
        event->accept();
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

// =============================================================================
// SkinPeelerScene
// =============================================================================
SkinPeelerScene::SkinPeelerScene(QObject* parent) : QGraphicsScene(parent) {}

void SkinPeelerScene::displayCTImage(const CTImage& img) {
    clear();
    m_pixmapItem = nullptr;
    m_maskItem = nullptr;
    m_shellOverlayItem = nullptr;
    m_annotationOverlayItem = nullptr;

    m_imageWidth = img.width();
    m_imageHeight = img.height();

    if (img.image().empty())
        return;

    QImage qimg(reinterpret_cast<const uchar*>(img.image().data()), m_imageWidth, m_imageHeight,
                m_imageWidth * 2, QImage::Format_Grayscale16);

    m_pixmapItem = addPixmap(QPixmap::fromImage(qimg.copy()));
}

void SkinPeelerScene::setShellOverlay(const std::vector<CTShell>& shells, int maxShell,
                                      const QColor& color, PeelRegion region) {
    clearShellOverlays();
    if (m_imageWidth == 0 || m_imageHeight == 0)
        return;

    QImage overlay(m_imageWidth, m_imageHeight, QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);

    const QRgb rgba = color.rgba();
    const int halfH = m_imageHeight / 2;
    const int halfW = m_imageWidth / 2;

    for (int si = 0; si <= maxShell && si < static_cast<int>(shells.size()); ++si) {
        for (const auto& v : shells[si]) {
            auto [sx, sy] =
                CoordinateTransform::voronoiToScene(v.x, v.y, m_imageHeight, m_imageWidth);
            int col = static_cast<int>(sx);
            int row = static_cast<int>(sy);

            if (col < 0 || col >= m_imageWidth || row < 0 || row >= m_imageHeight)
                continue;

            switch (region) {
                case PeelRegion::UpperHalf:
                    if (row >= halfH)
                        continue;
                    break;
                case PeelRegion::LowerHalf:
                    if (row < halfH)
                        continue;
                    break;
                case PeelRegion::LeftHalf:
                    if (col >= halfW)
                        continue;
                    break;
                case PeelRegion::RightHalf:
                    if (col < halfW)
                        continue;
                    break;
                case PeelRegion::WholeBody:
                    break;
            }
            overlay.setPixel(col, row, rgba);
        }
    }

    m_shellOverlayItem = addPixmap(QPixmap::fromImage(overlay));
}

void SkinPeelerScene::clearShellOverlays() {
    if (m_shellOverlayItem) {
        removeItem(m_shellOverlayItem);
        delete m_shellOverlayItem;
        m_shellOverlayItem = nullptr;
    }
}

void SkinPeelerScene::addVisualizationMask(const QRectF& rect) {
    clearVisualizationMask();
    m_maskItem = addRect(rect, QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 200)));
}

void SkinPeelerScene::clearVisualizationMask() {
    if (m_maskItem) {
        removeItem(m_maskItem);
        delete m_maskItem;
        m_maskItem = nullptr;
    }
}

// =============================================================================
// Annotation
// =============================================================================

void SkinPeelerScene::initAnnotation() {
    clearAnnotation();
    if (m_imageWidth > 0 && m_imageHeight > 0) {
        m_annotationMask = QImage(m_imageWidth, m_imageHeight, QImage::Format_Grayscale8);
        m_annotationMask.fill(0);
    }
}

bool SkinPeelerScene::toggleAnnotation(int col, int row) {
    if (m_annotationMask.isNull())
        return false;
    if (col < 0 || col >= m_imageWidth || row < 0 || row >= m_imageHeight)
        return false;

    uchar* line = m_annotationMask.scanLine(row);
    bool wasSet = (line[col] != 0);
    line[col] = wasSet ? 0 : 255;

    updateAnnotationOverlay();
    return !wasSet;  // returns new state
}

void SkinPeelerScene::clearAnnotation() {
    if (m_annotationOverlayItem) {
        removeItem(m_annotationOverlayItem);
        delete m_annotationOverlayItem;
        m_annotationOverlayItem = nullptr;
    }
    m_annotationMask = QImage();
}

bool SkinPeelerScene::hasAnnotation() const {
    if (m_annotationMask.isNull())
        return false;
    // Quick check: scan for any non-zero pixel
    for (int y = 0; y < m_annotationMask.height(); ++y) {
        const uchar* row = m_annotationMask.scanLine(y);
        for (int x = 0; x < m_annotationMask.width(); ++x) {
            if (row[x] != 0)
                return true;
        }
    }
    return false;
}

void SkinPeelerScene::updateAnnotationOverlay() {
    if (m_annotationOverlayItem) {
        removeItem(m_annotationOverlayItem);
        delete m_annotationOverlayItem;
        m_annotationOverlayItem = nullptr;
    }

    if (m_annotationMask.isNull())
        return;

    // Yellow semi-transparent overlay where mask = 255
    QImage overlay(m_imageWidth, m_imageHeight, QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);

    const QRgb yellow = qRgba(255, 255, 0, 180);
    for (int y = 0; y < m_imageHeight; ++y) {
        const uchar* maskRow = m_annotationMask.scanLine(y);
        for (int x = 0; x < m_imageWidth; ++x) {
            if (maskRow[x] != 0) {
                overlay.setPixel(x, y, yellow);
            }
        }
    }

    m_annotationOverlayItem = addPixmap(QPixmap::fromImage(overlay));
}

QImage SkinPeelerScene::exportAnnotatedImage(const CTImage& img) const {
    int w = img.width(), h = img.height();
    QImage result(w, h, QImage::Format_RGB888);

    const auto& pixels = img.image();
    double maxVal = img.imageMax();
    if (maxVal <= 0)
        maxVal = 1.0;

    for (int y = 0; y < h; ++y) {
        const uchar* maskRow = (!m_annotationMask.isNull() && y < m_annotationMask.height())
                                   ? m_annotationMask.constScanLine(y)
                                   : nullptr;
        for (int x = 0; x < w; ++x) {
            uint8_t gray =
                static_cast<uint8_t>(255.0 * static_cast<double>(pixels[y * w + x]) / maxVal);

            bool annotated = maskRow && x < m_annotationMask.width() && maskRow[x] != 0;

            if (annotated) {
                result.setPixel(x, y, qRgb(255, 255, 0));
            } else {
                result.setPixel(x, y, qRgb(gray, gray, gray));
            }
        }
    }
    return result;
}

QImage SkinPeelerScene::exportMaskImage() const {
    if (m_annotationMask.isNull())
        return QImage();
    return m_annotationMask.copy();
}

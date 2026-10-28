#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QColor>
#include <QPoint>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QImage>
#include <vector>
#include <functional>

#include "config/Settings.h"  // PeelRegion

class CTImage;
class CTShell;
class QGraphicsPixmapItem;
class QGraphicsRectItem;

// =============================================================================
// SkinPeelerView — QGraphicsView with zoom + pan/annotation modes
// =============================================================================
class SkinPeelerView : public QGraphicsView {
    Q_OBJECT
public:
    explicit SkinPeelerView(QWidget* parent = nullptr);

    void setAnnotationMode(bool on) {
        m_annotationMode = on;
    }
    bool annotationMode() const {
        return m_annotationMode;
    }

signals:
    // Emitted when the user paints in annotation mode (scene coordinates)
    void annotationPaint(QPointF scenePos);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool m_panning = false;
    bool m_annotationMode = false;
    QPoint m_lastPanPoint;
};

// =============================================================================
// SkinPeelerScene
// =============================================================================
class SkinPeelerScene : public QGraphicsScene {
    Q_OBJECT

public:
    explicit SkinPeelerScene(QObject* parent = nullptr);

    void displayCTImage(const CTImage& img);

    void setShellOverlay(const std::vector<CTShell>& shells, int maxShell, const QColor& color,
                         PeelRegion region = PeelRegion::WholeBody);
    void clearShellOverlays();

    void addVisualizationMask(const QRectF& rect);
    void clearVisualizationMask();

    // -------------------------------------------------------------------------
    // Annotation (expert segmentation)
    // -------------------------------------------------------------------------

    // Initialize annotation layer for current image dimensions
    void initAnnotation();

    // Toggle single pixel annotation at (col, row). Returns new state.
    bool toggleAnnotation(int col, int row);

    // Clear all annotations for current slice
    void clearAnnotation();

    // Has any annotation been made?
    bool hasAnnotation() const;

    // Export annotated CT as RGB PNG (grayscale CT + yellow annotation)
    QImage exportAnnotatedImage(const CTImage& img) const;

    // Export binary mask (white=annotated, black=not)
    QImage exportMaskImage() const;

    int imageWidth() const {
        return m_imageWidth;
    }
    int imageHeight() const {
        return m_imageHeight;
    }

private:
    void updateAnnotationOverlay();

    QGraphicsPixmapItem* m_pixmapItem = nullptr;
    QGraphicsPixmapItem* m_shellOverlayItem = nullptr;
    QGraphicsPixmapItem* m_annotationOverlayItem = nullptr;
    QGraphicsRectItem* m_maskItem = nullptr;

    int m_imageWidth = 0;
    int m_imageHeight = 0;

    // Annotation mask: 8-bit grayscale, 255=annotated, 0=not
    QImage m_annotationMask;
};

#pragma once

#include <QMainWindow>
#include <QColor>
#include <QPushButton>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <QProgressBar>
#include <QCloseEvent>

#include "models/CTImage.h"
#include "views/PlotWidgets.h"
#include "views/PlotWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class SkinPeelerScene;
class SkinPeelerView;

enum class VisualizationMode { WholeBody, UpperHalf, LowerHalf, LeftHalf, RightHalf };

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    // File I/O
    void onOpenDICOM();
    void onSaveDICOM();
    void onExportStatistics();

    // Navigation (keyboard arrows -> onPrev/onNext, lineEdit_CurrSection Enter -> onMoveToIndex)
    void onPrev();
    void onNext();
    void onMoveToIndex();

    // Peeling mode
    void onAutomaticModeClicked();
    void onCustomizeIntervalModeClicked();
    void onIntervalChanged();

    // Peeling parameters
    void onDetectionProbeChanged(double value);  // beta spinbox -> immediate recalc
    void onAnalysis();                           // Analysis button -> auto-detect thickness
    void onSkinThicknessEdited(double value);    // thickness spinbox manual edit -> overlay update

    // Visualization
    void onVisualizeWholeBody();
    void onVisualizeUpperHalf();
    void onVisualizeLowerHalf();
    void onVisualizeLeftHalf();
    void onVisualizeRightHalf();
    void onShowSkinToggled(bool checked);
    void onSkinColorPicker();

    // Peeling actions
    void onPeelCurrentSlice();
    void onPeelAllFiles();

    // Help
    void onShowManual();

    // Annotation (expert segmentation)
    void onAnnotationToggle(bool checked);
    void onAnnotationPaint(QPointF scenePos);
    void onAnnotationClear();
    void onAnnotationExport();

    // Async shell calculation
    void onShellCalcFinished(bool success, const std::string& errMsg);

    // Peel All — cancel button (slice-boundary cancellation)
    void onPeelAllCancelClicked();

private:
    // UI helpers
    void resetUIState();
    void activateUIAfterLoad();
    void updateImageIndexUI();
    void updateStatsBrowser();
    void applySkinColorToButton();
    // Sync skin thickness spinbox to match img->peelIndex() (1-based display).
    // Blocks valueChanged signal during update to avoid re-entrant setPeelIndex calls.
    void syncThicknessSpinbox(const CTImage* img);

    // Drawing
    void drawCurrentCT();
    void drawShellOverlays();

    // Internal helpers
    CTImage* currentImage();
    double currentBeta() const;
    void navigateToImage(int index);
    void startSliceShellCalc(int index);
    PeelRegion currentPeelRegion() const;
    void continueAnalysis(CTImage* img);
    void setUIEnabled(bool enabled);

    // -------------------------------------------------------------------------
    // runEngineTask — shared background-task runner.
    //
    // *** Concurrency contract ***
    // CTImage and the underlying V-ImageProcessing Voronoi/beta-complex engine
    // are NOT thread-safe (the library may hold process-wide state shared
    // across CTImage instances). Therefore AT MOST ONE engine-touching
    // background task (shell calculation, peeling, DICOM load+first-slice
    // calc) may be in flight at any time. m_calcRunning is the single flag
    // enforcing this for the whole window:
    //   - runEngineTask() sets it true for the duration of `work` and resets
    //     it to false (on the GUI thread, before `onDone` runs) on completion.
    //   - Callers must not start a second task while m_calcRunning is true
    //     (UI is disabled via setUIEnabled(false) to block most entry points;
    //     navigateToImage() additionally guards the keyboard-shortcut path,
    //     which bypasses the disabled centralWidget).
    //   - While m_calcRunning is true, the GUI thread must not call ANY
    //     method on ANY CTImage object (not just the one being processed) —
    //     this keeps the safety argument simple instead of relying on
    //     per-object reasoning about which slice the worker thread currently
    //     holds.
    // `work` runs on a detached std::thread. `onDone(success, errorMessage)`
    // is invoked back on the GUI thread (Qt::QueuedConnection). After
    // `onDone` returns, if a navigation request was queued while busy
    // (m_pendingNavIndex >= 0), it is replayed via navigateToImage().
    void runEngineTask(std::function<void()> work,
                       std::function<void(bool success, const std::string& errMsg)> onDone);

    // Members
    Ui::MainWindow* m_ui = nullptr;
    SkinPeelerScene* m_scene = nullptr;

    std::vector<std::unique_ptr<CTImage>> m_images;
    int m_currentIndex = 0;
    int m_intervalSections = 5;
    bool m_peeled = false;
    bool m_viewInitialized = false;  // true after first fitInView

    VisualizationMode m_vizMode = VisualizationMode::WholeBody;
    QColor m_skinColor = QColor(255, 255, 0);  // yellow

    QProgressBar* m_progressBar = nullptr;
    std::atomic<bool> m_calcRunning{false};

    // Latest slice index requested by navigateToImage() while m_calcRunning
    // was true; -1 = no pending request. Replayed by runEngineTask() once
    // the in-flight task completes (coalesces rapid navigation into a single
    // trailing request instead of queuing every intermediate slice).
    int m_pendingNavIndex = -1;

    // Peel All — cancel button + request flag (polled by PeelingService
    // between slices; cancellation takes effect at the next slice boundary).
    QPushButton* m_btnCancelPeel = nullptr;
    std::atomic<bool> m_peelCancelRequested{false};

    // Annotation UI (created programmatically, not in .ui)
    QPushButton* m_btnAnnotate = nullptr;
    QPushButton* m_btnAnnClear = nullptr;
    QPushButton* m_btnAnnExport = nullptr;
};

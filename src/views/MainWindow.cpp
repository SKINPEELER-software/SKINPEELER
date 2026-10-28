#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "SkinPeelerScene.h"
#include "AnalysisDialog.h"
#include "PlotWindow.h"
#include "services/DicomService.h"
#include "services/PeelingService.h"
#include "services/StatisticsService.h"
#include "config/Settings.h"

#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QKeyEvent>
#include <QApplication>
#include <QStatusBar>
#include <QFileInfo>
#include <QEvent>
#include <QColorDialog>
#include <QTimer>
#include <QLabel>
#include <QToolBar>
#include <QImageWriter>

#include <algorithm>
#include <cmath>
#include <thread>
#include <string>

namespace {
// Indeterminate background tasks (shell calculation for a single slice, DICOM
// load, beta change) only show the progress bar if they run longer than this,
// to avoid flicker on the common fast case. Peel All uses a determinate bar
// shown immediately instead (see onPeelAllFiles).
constexpr int kBusyIndicatorDelayMs = 20000;
}

// =============================================================================
// Construction
// =============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    m_scene = new SkinPeelerScene(this);
    m_ui->graphicsView->setScene(m_scene);

    m_ui->lineEdit_CurrSection->setFocusPolicy(Qt::ClickFocus);
    m_ui->lineEdit_TotalSection->setFocusPolicy(Qt::ClickFocus);
    m_ui->graphicsView->installEventFilter(this);

    // File I/O
    connect(m_ui->pushButton_OpenDICOMfiles,   &QPushButton::clicked, this, &MainWindow::onOpenDICOM);
    connect(m_ui->pushButton_SaveChanges,      &QPushButton::clicked, this, &MainWindow::onSaveDICOM);
    connect(m_ui->pushButton_ExportStatistics, &QPushButton::clicked, this, &MainWindow::onExportStatistics);

    // Navigation
    connect(m_ui->lineEdit_CurrSection, &QLineEdit::returnPressed, this, &MainWindow::onMoveToIndex);

    // Peeling mode
    connect(m_ui->radioButton_Automatic,         &QRadioButton::clicked, this, &MainWindow::onAutomaticModeClicked);
    connect(m_ui->radioButton_CustomizeInterval, &QRadioButton::clicked, this, &MainWindow::onCustomizeIntervalModeClicked);
    connect(m_ui->lineEdit_Interval, &QLineEdit::returnPressed, this, &MainWindow::onIntervalChanged);

    // Peeling parameters
    connect(m_ui->doubleSpinBox_DetectionProbe,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onDetectionProbeChanged);
    connect(m_ui->pushButton_Analysis, &QPushButton::clicked, this, &MainWindow::onAnalysis);
    connect(m_ui->doubleSpinBox_SkinThickness,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onSkinThicknessEdited);

    // Visualization
    connect(m_ui->radioButton_WholeBody,  &QRadioButton::clicked, this, &MainWindow::onVisualizeWholeBody);
    connect(m_ui->radioButton_UpperHalf,  &QRadioButton::clicked, this, &MainWindow::onVisualizeUpperHalf);
    connect(m_ui->radioButton_LowerHalf,  &QRadioButton::clicked, this, &MainWindow::onVisualizeLowerHalf);
    connect(m_ui->radioButton_LeftHalf,   &QRadioButton::clicked, this, &MainWindow::onVisualizeLeftHalf);
    connect(m_ui->radioButton_RightHalf,  &QRadioButton::clicked, this, &MainWindow::onVisualizeRightHalf);
    connect(m_ui->checkBox_ShowSkin,  &QCheckBox::toggled, this, &MainWindow::onShowSkinToggled);
    connect(m_ui->pushButton_SkinColor, &QPushButton::clicked, this, &MainWindow::onSkinColorPicker);

    // Peeling actions
    connect(m_ui->pushButton_PeelCurrentSlice, &QPushButton::clicked, this, &MainWindow::onPeelCurrentSlice);
    connect(m_ui->pushButton_PeelAllFiles,     &QPushButton::clicked, this, &MainWindow::onPeelAllFiles);

    // Help
    connect(m_ui->actionUserGuide, &QAction::triggered, this, &MainWindow::onShowManual);

    // Progress bar (bottom-right, hidden initially)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0);
    m_progressBar->setFixedWidth(180);
    m_progressBar->setFixedHeight(16);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);

    // Cancel button for Peel All (slice-boundary cancellation). Hidden except
    // while a Peel All operation is running.
    m_btnCancelPeel = new QPushButton("Cancel", this);
    m_btnCancelPeel->setFixedHeight(20);
    m_btnCancelPeel->setVisible(false);
    statusBar()->addPermanentWidget(m_btnCancelPeel);
    connect(m_btnCancelPeel, &QPushButton::clicked, this, &MainWindow::onPeelAllCancelClicked);

    // --- Annotation toolbar (added programmatically) ---
    {
        auto* toolbar = addToolBar("Annotation");
        toolbar->setMovable(false);

        m_btnAnnotate = new QPushButton("Annotate", this);
        m_btnAnnotate->setCheckable(true);
        m_btnAnnotate->setToolTip("Toggle annotation mode: click pixel to mark/unmark as skin");
        toolbar->addWidget(m_btnAnnotate);

        m_btnAnnClear = new QPushButton("Clear", this);
        m_btnAnnClear->setToolTip("Clear annotation on current slice");
        toolbar->addWidget(m_btnAnnClear);

        m_btnAnnExport = new QPushButton("Export Annotation", this);
        m_btnAnnExport->setToolTip("Save annotated PNG + binary mask");
        toolbar->addWidget(m_btnAnnExport);

        connect(m_btnAnnotate,  &QPushButton::toggled, this, &MainWindow::onAnnotationToggle);
        connect(m_btnAnnClear,  &QPushButton::clicked, this, &MainWindow::onAnnotationClear);
        connect(m_btnAnnExport, &QPushButton::clicked, this, &MainWindow::onAnnotationExport);

        connect(m_ui->graphicsView, &SkinPeelerView::annotationPaint,
                this, &MainWindow::onAnnotationPaint);
    }

    resetUIState();
    applySkinColorToButton();
}

MainWindow::~MainWindow()
{
    delete m_ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_calcRunning) {
        // Block close during shell calculation to prevent dangling pointer
        QMessageBox::information(this, "Calculating",
            "Shell calculation is in progress. Please wait until it completes.");
        event->ignore();
    } else {
        event->accept();
    }
}

// =============================================================================
// File I/O
// =============================================================================

void MainWindow::onOpenDICOM()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Open DICOM Folder", QDir::currentPath());
    if (dir.isEmpty()) return;

    // Defensive — UI is disabled while m_calcRunning, so this button should
    // not be reachable, but guard anyway per the single-concurrent-op rule.
    if (m_calcRunning) return;

    m_images.clear();
    m_peeled = false;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    setUIEnabled(false);
    statusBar()->showMessage("Loading DICOM files...");
    m_progressBar->setRange(0, 0);  // indeterminate (Peel All leaves it determinate)

    QTimer::singleShot(kBusyIndicatorDelayMs, this, [this]() {
        if (m_calcRunning) m_progressBar->setVisible(true);
    });

    // DicomService::loadDirectory() + the first slice's calculateShells() run
    // entirely on the worker thread. The loaded vector is staged in a
    // shared_ptr (not a member) so the worker thread never touches m_images —
    // it is assigned on the GUI thread only after the task fully completes.
    const std::string dirStd = dir.toStdString();
    const double beta = currentBeta();
    auto loadedImages = std::make_shared<std::vector<std::unique_ptr<CTImage>>>();

    runEngineTask(
        [dirStd, beta, loadedImages]() {
            *loadedImages = DicomService::loadDirectory(dirStd, beta);
            if (!loadedImages->empty()) {
                // Auto-calculate shells for the first slice so it is ready
                // to display immediately once loading completes.
                loadedImages->front()->calculateShells();
            }
        },
        [this, dir, loadedImages](bool ok, const std::string& errMsg) {
            QApplication::restoreOverrideCursor();
            m_progressBar->setVisible(false);
            setUIEnabled(true);

            if (!ok) {
                loadedImages->clear();
                QMessageBox::warning(this, "Load Error", QString::fromStdString(errMsg));
                statusBar()->showMessage("Load failed.");
                return;
            }

            m_images = std::move(*loadedImages);

            if (m_images.empty()) {
                QMessageBox::warning(this, "No Files",
                    "No valid DICOM files found in the selected folder.");
                statusBar()->clearMessage();
                return;
            }

            m_currentIndex = 0;
            resetUIState();
            activateUIAfterLoad();
            drawCurrentCT();

            // calculateShells() (run on the worker thread above) sets
            // img->peelIndex() via calculateCuttingShell(). syncThicknessSpinbox()
            // updates the spinbox to the detected 1-based count so the UI
            // matches the actual rendered shell count.
            CTImage* img = currentImage();
            if (img && img->hasShells()) {
                syncThicknessSpinbox(img);
                drawShellOverlays();
            }

            m_ui->graphicsView->setFocus();
            statusBar()->showMessage(
                QString("Loaded %1 slices from %2").arg(m_images.size()).arg(dir));
        });
}

void MainWindow::onSaveDICOM()
{
    if (m_images.empty()) return;

    QString dir = QFileDialog::getExistingDirectory(
        this, "Save Peeled DICOM", QDir::currentPath());
    if (dir.isEmpty()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage("Saving...");

    int saved = DicomService::saveDirectory(m_images, dir.toStdString());

    QApplication::restoreOverrideCursor();
    statusBar()->showMessage(
        QString("Saved %1 / %2 files to %3").arg(saved).arg(m_images.size()).arg(dir));
}

void MainWindow::onExportStatistics()
{
    if (m_images.empty()) return;

    QString path = QFileDialog::getSaveFileName(
        this, "Export Statistics", QDir::currentPath(), "Text files (*.txt)");
    if (path.isEmpty()) return;

    auto stats = StatisticsService::collect(m_images);
    if (!StatisticsService::saveToFile(path.toStdString(), stats)) {
        QMessageBox::warning(this, "Save Error", "Failed to write statistics file.");
    } else {
        statusBar()->showMessage(QString("Statistics exported to %1").arg(path));
    }
}

// =============================================================================
// Navigation
// =============================================================================

void MainWindow::onPrev()
{
    if (m_currentIndex > 0)
        navigateToImage(m_currentIndex - 1);
}

void MainWindow::onNext()
{
    if (m_currentIndex < static_cast<int>(m_images.size()) - 1)
        navigateToImage(m_currentIndex + 1);
}

void MainWindow::onMoveToIndex()
{
    int idx = m_ui->lineEdit_CurrSection->text().toInt() - 1;
    idx = std::clamp(idx, 0, static_cast<int>(m_images.size()) - 1);
    navigateToImage(idx);
}

void MainWindow::navigateToImage(int index)
{
    if (m_images.empty() || index < 0 || index >= static_cast<int>(m_images.size()))
        return;

    if (m_calcRunning) {
        // See runEngineTask()'s concurrency contract: the engine is in use by
        // another background task, so the GUI thread must not touch any
        // CTImage right now (not even a read on a different slice). Keyboard
        // Prev/Next bypasses the disabled centralWidget, so this guard is the
        // actual enforcement point for that path.
        //
        // Record only the latest requested index — fast repeated navigation
        // (e.g. holding an arrow key) must coalesce into a single eventual
        // calculation instead of queuing every intermediate slice.
        m_pendingNavIndex = index;
        statusBar()->showMessage("Busy — navigation will resume once the current operation finishes...");
        return;
    }

    m_currentIndex = index;
    updateImageIndexUI();
    drawCurrentCT();

    CTImage* img = currentImage();
    if (!img) return;

    // Bug 3: Sync beta — if spinbox value differs from stored beta, update and clear shells.
    double spinBeta = currentBeta();
    if (std::abs(img->beta() - spinBeta) > 1e-9) {
        img->setBeta(spinBeta);  // clears existing shells for this image
    }

    if (!img->hasShells()) {
        startSliceShellCalc(index);
        return;
    }

    // Sync spinbox after potential shell recalculation so the displayed
    // thickness always matches the internally stored peelIndex.
    syncThicknessSpinbox(img);
    drawShellOverlays();
}

void MainWindow::startSliceShellCalc(int index)
{
    if (index < 0 || index >= static_cast<int>(m_images.size())) return;
    CTImage* img = m_images[index].get();
    if (!img) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    setUIEnabled(false);
    statusBar()->showMessage(QString("Calculating shells for slice %1...").arg(index + 1));
    m_progressBar->setRange(0, 0);  // indeterminate

    QTimer::singleShot(kBusyIndicatorDelayMs, this, [this]() {
        if (m_calcRunning) m_progressBar->setVisible(true);
    });

    runEngineTask(
        [img]() { img->calculateShells(); },
        [this, index](bool ok, const std::string& errMsg) {
            QApplication::restoreOverrideCursor();
            m_progressBar->setVisible(false);
            setUIEnabled(true);

            if (!ok) {
                QMessageBox::warning(this, "Shell Calculation Error",
                    QString("Failed to calculate shells:\n%1").arg(QString::fromStdString(errMsg)));
                statusBar()->showMessage("Shell calculation failed.");
                return;
            }

            // The user may have navigated away while this was running; the
            // pending-navigation replay in runEngineTask() handles that, so
            // only refresh the overlay if we are still looking at this slice.
            if (index == m_currentIndex) {
                CTImage* cur = currentImage();
                if (cur) {
                    syncThicknessSpinbox(cur);
                    drawShellOverlays();
                }
                statusBar()->clearMessage();
            }
        });
}

// =============================================================================
// Peeling mode
// =============================================================================

void MainWindow::onAutomaticModeClicked()
{
    m_ui->lineEdit_Interval->setEnabled(false);
}

void MainWindow::onCustomizeIntervalModeClicked()
{
    m_ui->lineEdit_Interval->setEnabled(true);
}

void MainWindow::onIntervalChanged()
{
    bool ok = false;
    int val = m_ui->lineEdit_Interval->text().toInt(&ok);
    if (ok && val > 0) {
        m_intervalSections = val;
    }
}

// =============================================================================
// Peeling parameters
// =============================================================================

void MainWindow::onDetectionProbeChanged(double value)
{
    CTImage* img = currentImage();
    if (!img) return;

    // Defensive — the spinbox lives in centralWidget and is disabled while
    // m_calcRunning, so this should not be reachable mid-task.
    if (m_calcRunning) return;

    img->setBeta(value);  // internally clears shells (cheap, no engine call)

    QApplication::setOverrideCursor(Qt::WaitCursor);
    setUIEnabled(false);
    statusBar()->showMessage("Recalculating shells for new beta...");
    m_progressBar->setRange(0, 0);  // indeterminate

    QTimer::singleShot(kBusyIndicatorDelayMs, this, [this]() {
        if (m_calcRunning) m_progressBar->setVisible(true);
    });

    runEngineTask(
        [img]() { img->calculateShells(); },
        [this, img](bool ok, const std::string& errMsg) {
            QApplication::restoreOverrideCursor();
            m_progressBar->setVisible(false);
            setUIEnabled(true);

            if (!ok) {
                QMessageBox::warning(this, "Shell Calculation Error",
                    QString("Failed to calculate shells:\n%1").arg(QString::fromStdString(errMsg)));
                statusBar()->showMessage("Shell calculation failed.");
                return;
            }

            // Beta change triggers full shell recalculation -> peelIndex changes.
            // Sync spinbox so the displayed thickness matches the new result.
            // img remains valid: m_images is not resized/touched by any other
            // path while m_calcRunning was true.
            if (img == currentImage()) {
                syncThicknessSpinbox(img);
                drawCurrentCT();
                drawShellOverlays();
                statusBar()->clearMessage();
            }
        });
}

void MainWindow::onAnalysis()
{
    CTImage* img = currentImage();
    if (!img) return;

    // Defensive — the Analysis button lives in centralWidget and is disabled
    // while m_calcRunning, so this should not be reachable mid-task.
    if (m_calcRunning) return;

    // calculateShells() always maximizes. If shells already exist, reuse them.
    if (img->hasShells()) {
        continueAnalysis(img);
        return;
    }

    setUIEnabled(false);
    statusBar()->showMessage("Calculating shells... (this may take a while)");
    m_progressBar->setRange(0, 0);  // indeterminate
    m_progressBar->setVisible(false);

    QTimer::singleShot(kBusyIndicatorDelayMs, this, [this]() {
        if (m_calcRunning) {
            m_progressBar->setVisible(true);
        }
    });

    runEngineTask(
        [img]() { img->calculateShells(); },
        [this](bool ok, const std::string& errMsg) { onShellCalcFinished(ok, errMsg); });
}

void MainWindow::onShellCalcFinished(bool success, const std::string& errMsg)
{
    // m_calcRunning is reset by runEngineTask() before this runs.
    m_progressBar->setVisible(false);
    setUIEnabled(true);

    if (!success) {
        QMessageBox::warning(this, "Shell Calculation Error",
            QString("Failed to calculate shells:\n%1").arg(QString::fromStdString(errMsg)));
        statusBar()->showMessage("Shell calculation failed.");
        return;
    }

    CTImage* img = currentImage();
    if (!img) return;

    continueAnalysis(img);
}

void MainWindow::continueAnalysis(CTImage* img)
{
    // extractShellStatistics(excludeOutermost=true) returns a stats array where
    // index 0 corresponds to m_shells[1] (the outermost shell is excluded).
    // GaussianPlotWidget and ScatterPlotWidget consume stats-array indices, so
    // we must convert from m_shells index space to stats index space:
    //   stats_index = m_shells_index - statsOffset
    // where statsOffset = 1 when EXCLUDE_OUTERMOST_SHELL is true, 0 otherwise.
    // algoPeelIndex() is an m_shells index → convert to stats index.
    int calcPeelIdx = img->algoPeelIndex() - ShellIndex::kOffset;

    // Spinbox now shows s-label number (= stats-array index + 1).
    // spinbox value 1 → stats index 0 (s1 in stats space)
    // spinbox value 2 → stats index 1 (s2 in stats space)
    // No statsOffset subtraction needed since spinbox is already in s-label space.
    int currThickness = static_cast<int>(m_ui->doubleSpinBox_SkinThickness->value()) - 1;

    drawCurrentCT();
    drawShellOverlays();

    auto stats = img->extractShellStatistics();

    // Clamp both indices to valid stats-array range.
    // stats is already in stats-index space (excludeOutermost applied).
    {
        const int maxStatsIdx = std::max(0, static_cast<int>(stats.size()) - 1);
        calcPeelIdx   = std::clamp(calcPeelIdx,   0, maxStatsIdx);
        currThickness = std::clamp(currThickness, 0, maxStatsIdx);
    }

    // Status bar: report using s-label number (consistent with spinbox display).
    // algoPeelIndex() is an m_shells index → s-label = algoPeelIndex - statsOffset + 1.
    {
        const int shellLabelNum = ShellIndex::toLabel(img->algoPeelIndex());
        QString statusMsg = QString("Analysis complete. Detected skin thickness: %1 shells (s%1)")
                                .arg(shellLabelNum);
        if (img->timedOut())
            statusMsg += " [time limit reached — partial shells]";
        statusBar()->showMessage(statusMsg);
    }

    // --- Update plainTextEdit_Statistics ---
    {
        const auto& shells = img->shells();
        int totalShells = static_cast<int>(shells.size());

        QString report;
        report += "=== Analysis Results ===\n";
        report += QString("Total shells: %1\n").arg(totalShells);
        if (img->timedOut())
            report += QString("[Note: shell calculation stopped at time limit]\n");
        report += QString("Algorithm cut: s%1 (0-based index %2)\n")
                      .arg(calcPeelIdx + 1).arg(calcPeelIdx);
        report += QString("Current user setting: s%1 (0-based index %2)\n\n")
                      .arg(currThickness + 1).arg(currThickness);

        report += "Shell Stats (shell#: mu +/- sigma):\n";
        int displayLimit = std::min(10, totalShells);
        for (int i = 0; i < displayLimit; ++i) {
            const CTShell& sh = shells[i];
            if (sh.hasStatistics()) {
                report += QString("  s%1: %2 +/- %3\n")
                              .arg(i + 1)
                              .arg(sh.mu(),    0, 'f', 4)
                              .arg(sh.sigma(), 0, 'f', 4);
            } else {
                report += QString("  s%1: (no stats)\n").arg(i + 1);
            }
        }
        if (totalShells > 10)
            report += QString("  ... (%1 more shells omitted)\n").arg(totalShells - 10);

        {
            auto distStats = StatisticsService::computeInterShellDistances(stats);
            if (distStats.valid) {
                report += "\nInter-shell distances:\n";
                report += QString("  Mean: %1\n").arg(distStats.mean, 0, 'f', 4);
                report += QString("  Max:  %1\n").arg(distStats.max,  0, 'f', 4);
                report += QString("  Min:  %1\n").arg(distStats.min,  0, 'f', 4);
            }
        }

        m_ui->groupBox_Statistics->setEnabled(true);
        m_ui->plainTextEdit_Statistics->setPlainText(report);
    }

    if (!stats.empty()) {
        auto* gaussPlot   = new GaussianPlotWidget(stats, calcPeelIdx, currThickness);
        auto* scatterPlot = new ScatterPlotWidget(stats, calcPeelIdx, currThickness);
        auto* histPlot    = new HistogramWidget(stats);

        auto* w1 = new PlotWindow(gaussPlot,   "Gaussian Distribution",  this);
        auto* w2 = new PlotWindow(scatterPlot, "Shell-wise Profile",      this);
        auto* w3 = new PlotWindow(histPlot,    "Inter-Shell Distance",    this);

        w1->setAttribute(Qt::WA_DeleteOnClose);
        w2->setAttribute(Qt::WA_DeleteOnClose);
        w3->setAttribute(Qt::WA_DeleteOnClose);

        w1->move(100, 100);  w1->show();
        w2->move(820, 100);  w2->show();
        w3->move(100, 600);  w3->show();
    }
}

void MainWindow::setUIEnabled(bool enabled)
{
    // Disable/enable the central widget to block all user interaction
    // during background shell calculation
    if (centralWidget())
        centralWidget()->setEnabled(enabled);

    // The Annotation toolbar is docked directly on the QMainWindow (added via
    // addToolBar()), so its buttons are NOT descendants of centralWidget and
    // would otherwise stay clickable during a background engine task. Their
    // handlers read/write CTImage state, so they must be gated the same way.
    if (m_btnAnnotate)  m_btnAnnotate->setEnabled(enabled);
    if (m_btnAnnClear)  m_btnAnnClear->setEnabled(enabled);
    if (m_btnAnnExport) m_btnAnnExport->setEnabled(enabled);
}

void MainWindow::onSkinThicknessEdited(double value)
{
    CTImage* img = currentImage();
    if (!img || !img->hasShells()) return;
    if (value < 1.0) return;  // 0 = "not set yet" sentinel; ignore

    // Spinbox shows s-label number → convert to 0-based m_shells peelIndex
    int t = ShellIndex::toShellsIdx(static_cast<int>(value));
    t = std::max(0, std::min(t, static_cast<int>(img->shells().size()) - 1));
    img->setPeelIndex(t);
    drawShellOverlays();   // 재계산 없이 overlay만 업데이트
}

// =============================================================================
// Visualization
// =============================================================================

void MainWindow::onVisualizeWholeBody()  { m_vizMode = VisualizationMode::WholeBody;  drawShellOverlays(); }
void MainWindow::onVisualizeUpperHalf()  { m_vizMode = VisualizationMode::UpperHalf;  drawShellOverlays(); }
void MainWindow::onVisualizeLowerHalf()  { m_vizMode = VisualizationMode::LowerHalf;  drawShellOverlays(); }
void MainWindow::onVisualizeLeftHalf()   { m_vizMode = VisualizationMode::LeftHalf;   drawShellOverlays(); }
void MainWindow::onVisualizeRightHalf()  { m_vizMode = VisualizationMode::RightHalf;  drawShellOverlays(); }

void MainWindow::onShowSkinToggled(bool checked)
{
    if (checked)
        drawShellOverlays();
    else
        m_scene->clearShellOverlays();
}

void MainWindow::onSkinColorPicker()
{
    QColor color = QColorDialog::getColor(m_skinColor, this, "Select Skin Color");
    if (color.isValid()) {
        m_skinColor = color;
        applySkinColorToButton();
        if (m_ui->checkBox_ShowSkin->isChecked())
            drawShellOverlays();
    }
}

// =============================================================================
// Peeling actions
// =============================================================================

void MainWindow::onPeelCurrentSlice()
{
    // Defensive — the button lives in centralWidget and is disabled while
    // m_calcRunning, so this should not be reachable mid-task. See
    // runEngineTask()'s concurrency contract in MainWindow.h.
    if (m_calcRunning) return;

    CTImage* img = currentImage();
    if (!img) return;

    // Spinbox shows s-label number → convert to 0-based m_shells peelIndex before peeling
    int dispThickness = static_cast<int>(m_ui->doubleSpinBox_SkinThickness->value());
    if (dispThickness >= 1) img->setPeelIndex(ShellIndex::toShellsIdx(dispThickness));

    img->peelSkin(currentPeelRegion());
    drawCurrentCT();

    // Bug 1: Peeled → uncheck Show Skin (overlay no longer meaningful on peeled image).
    // setChecked(false) emits toggled(false) → onShowSkinToggled(false) → clearShellOverlays().
    m_ui->checkBox_ShowSkin->setChecked(false);

    m_ui->pushButton_PeelAllFiles->setEnabled(true);
    statusBar()->showMessage(QString("Peeled slice %1").arg(m_currentIndex + 1));
}

void MainWindow::onPeelAllFiles()
{
    if (m_images.empty()) return;

    // Defensive — the button lives in centralWidget and is disabled while
    // m_calcRunning, so this should not be reachable mid-task.
    if (m_calcRunning) return;

    PeelRegion region = currentPeelRegion();
    const int total = static_cast<int>(m_images.size());
    const bool autoMode = m_ui->radioButton_Automatic->isChecked();

    // Plan (just integers) built on the GUI thread; the actual per-slice
    // engine work (calculateShells/peelSkin/releaseVoronoi, operating
    // directly on m_images) runs on the worker thread below. This is safe
    // only because the single-concurrent-op rule guarantees the GUI thread
    // will not touch m_images until the task's onDone callback runs.
    std::vector<int> sections;
    std::vector<int> thicknesses;
    if (!autoMode) {
        int globalThickness = ShellIndex::toShellsIdx(
            static_cast<int>(m_ui->doubleSpinBox_SkinThickness->value()));
        int numSections = std::max(1, std::min(m_intervalSections, total));
        sections = PeelingService::computeSectionIndices(total, numSections);
        thicknesses.assign(sections.size(), globalThickness);
    }

    m_peelCancelRequested = false;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    setUIEnabled(false);
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_btnCancelPeel->setVisible(true);
    m_btnCancelPeel->setEnabled(true);
    statusBar()->showMessage(QString("Peeling slice 0 / %1...").arg(total));

    // Progress callback is invoked from the worker thread between slices —
    // marshal to the GUI thread before touching any widget.
    PeelingService::ProgressCallback progressCb =
        [this](int completed, int taskTotal) {
            QMetaObject::invokeMethod(this, [this, completed, taskTotal]() {
                m_progressBar->setValue(completed);
                statusBar()->showMessage(
                    QString("Peeling slice %1 / %2...").arg(completed).arg(taskTotal));
            }, Qt::QueuedConnection);
        };
    PeelingService::CancelCallback cancelCb =
        [this]() { return m_peelCancelRequested.load(); };

    runEngineTask(
        [this, region, autoMode, sections, thicknesses, progressCb, cancelCb]() {
            if (autoMode) {
                PeelingService::peelAutoMode(m_images, 0, region, progressCb, cancelCb);
            } else {
                PeelingService::peelUniformMode(
                    m_images, sections, thicknesses, region, progressCb, cancelCb);
            }
        },
        [this](bool ok, const std::string& errMsg) {
            QApplication::restoreOverrideCursor();
            m_progressBar->setVisible(false);
            m_btnCancelPeel->setVisible(false);
            setUIEnabled(true);

            // Mark peeled even on partial/cancelled completion — whatever
            // slices were processed before the error/cancel are genuinely
            // peeled and should be reflected in the UI and saveable.
            m_peeled = true;
            drawCurrentCT();
            updateStatsBrowser();

            m_ui->pushButton_SaveChanges->setEnabled(true);
            m_ui->pushButton_ExportStatistics->setEnabled(true);
            m_ui->groupBox_Statistics->setEnabled(true);

            if (!ok) {
                QMessageBox::warning(this, "Peeling Error",
                    QString("Peeling stopped due to an error:\n%1")
                        .arg(QString::fromStdString(errMsg)));
                statusBar()->showMessage("Peeling failed.");
            } else if (m_peelCancelRequested) {
                statusBar()->showMessage("Peeling cancelled.");
            } else {
                statusBar()->showMessage("Peeling complete.");
            }
        });
}

void MainWindow::onPeelAllCancelClicked()
{
    // Polled by PeelingService between slices (see cancelCb in
    // onPeelAllFiles); takes effect at the next slice boundary.
    m_peelCancelRequested = true;
    m_btnCancelPeel->setEnabled(false);
    statusBar()->showMessage("Cancelling after current slice...");
}

// =============================================================================
// Help
// =============================================================================

void MainWindow::onShowManual()
{
    QMessageBox::information(this, "User Guide",
        "SKINPEELER 2.0\n\n"
        "1. Open DICOM files — load a folder of .dcm slice files.\n"
        "2. Choose Peeling Mode:\n"
        "   Automatic: thickness is detected per-slice automatically.\n"
        "   Customize Interval: divide slices into N key sections.\n"
        "3. Set Detection Probe (beta) if needed — triggers immediate recalculation.\n"
        "4. Press 'Analysis' to auto-detect skin thickness for the current slice.\n"
        "5. Press 'Peel Current Slice' to peel the current slice.\n"
        "6. Press 'Apply Skin Parameters to All Files' to peel all slices.\n"
        "7. Use Peeling Region to view specific body halves.\n"
        "8. Save Changes — export peeled slices as DICOM.\n"
        "9. Export Statistics — save per-slice statistics to .txt.\n\n"
        "Keyboard: Left/Right arrow — navigate slices."
    );
}

// =============================================================================
// Key events
// =============================================================================

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_ui->graphicsView && event->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right) {
            keyPressEvent(ke);
            return true;  // consumed — don't let graphicsView pan
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (m_images.empty()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Left) {
        onPrev();
    } else if (event->key() == Qt::Key_Right) {
        onNext();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

// =============================================================================
// UI helpers
// =============================================================================

void MainWindow::resetUIState()
{
    m_ui->groupbox_PeelingMode->setEnabled(false);
    m_ui->groupBox_PeelingParameters->setEnabled(false);
    m_ui->groupBox_VisualizeOptions->setEnabled(false);
    m_ui->groupBox_Statistics->setEnabled(false);
    m_ui->pushButton_PeelAllFiles->setEnabled(false);
    m_ui->pushButton_SaveChanges->setEnabled(false);
    m_ui->pushButton_ExportStatistics->setEnabled(false);

    m_ui->plainTextEdit_Statistics->clear();
    m_ui->lineEdit_CurrSection->clear();
    m_ui->lineEdit_TotalSection->clear();
    m_ui->label_CurrentDir->setText("Current Dir : ");
    m_ui->label_CurrentFIle->setText("Current File : ");
    m_ui->doubleSpinBox_SkinThickness->setValue(1.0);

    m_ui->radioButton_Automatic->setChecked(true);
    m_ui->lineEdit_Interval->setEnabled(false);
    m_ui->lineEdit_Interval->setText(QString::number(DEFAULT_INTERVAL_SECTIONS));
    m_ui->radioButton_WholeBody->setChecked(true);

    m_vizMode          = VisualizationMode::WholeBody;
    m_peeled           = false;
    m_intervalSections = DEFAULT_INTERVAL_SECTIONS;
    m_viewInitialized  = false;  // Bug 4: reset so next load triggers fitInView once
}

void MainWindow::activateUIAfterLoad()
{
    m_ui->groupbox_PeelingMode->setEnabled(true);
    m_ui->groupBox_PeelingParameters->setEnabled(true);
    m_ui->groupBox_VisualizeOptions->setEnabled(true);
    m_ui->pushButton_PeelAllFiles->setEnabled(true);

    m_ui->lineEdit_TotalSection->setText(QString::number(m_images.size()));
    updateImageIndexUI();

    m_ui->checkBox_ShowSkin->setChecked(true);  // Bug 1: Show Skin on by default after load
}

void MainWindow::updateImageIndexUI()
{
    if (m_images.empty()) return;

    m_ui->lineEdit_CurrSection->setText(QString::number(m_currentIndex + 1));

    const CTImage* img = m_images[m_currentIndex].get();
    QFileInfo fi(QString::fromStdString(img->filepath()));
    m_ui->label_CurrentFIle->setText("Current File : " + fi.fileName());
    m_ui->label_CurrentDir->setText("Current Dir : " + fi.absolutePath());
}

void MainWindow::updateStatsBrowser()
{
    auto stats = StatisticsService::collect(m_images);
    QString report = QString::fromStdString(StatisticsService::formatFullReport(stats));
    m_ui->groupBox_Statistics->setEnabled(true);
    m_ui->plainTextEdit_Statistics->setPlainText(report);
}

void MainWindow::applySkinColorToButton()
{
    m_ui->pushButton_SkinColor->setStyleSheet(
        QString("QPushButton { background-color: %1; }").arg(m_skinColor.name()));
}

void MainWindow::syncThicknessSpinbox(const CTImage* img)
{
    if (!img || !img->hasShells()) return;
    // peelIndex is 0-based m_shells index.
    // Spinbox shows s-label number = ShellIndex::toLabel(peelIndex).
    int display = ShellIndex::toLabel(img->peelIndex());
    if (display < 1) display = 1;
    // Block the valueChanged signal so setValue() does not trigger
    // onSkinThicknessEdited() — peelIndex is already correct.
    m_ui->doubleSpinBox_SkinThickness->blockSignals(true);
    m_ui->doubleSpinBox_SkinThickness->setValue(static_cast<double>(display));
    m_ui->doubleSpinBox_SkinThickness->blockSignals(false);
}

// =============================================================================
// Drawing
// =============================================================================

void MainWindow::drawCurrentCT()
{
    CTImage* img = currentImage();
    if (!img) return;
    m_scene->displayCTImage(*img);
    // Bug 4: fitInView only on the very first display after loading.
    // Subsequent slice navigation and peel preserve the current zoom/pan state.
    if (!m_viewInitialized) {
        m_ui->graphicsView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
        m_viewInitialized = true;
    }
}

void MainWindow::drawShellOverlays()
{
    m_scene->clearShellOverlays();
    if (!m_ui->checkBox_ShowSkin->isChecked()) return;

    CTImage* img = currentImage();
    if (!img || !img->hasShells()) return;

    int maxShell = std::min(img->peelIndex(), static_cast<int>(img->shells().size()) - 1);
    if (maxShell < 0) return;

    m_scene->setShellOverlay(
        img->shells(),
        maxShell,
        m_skinColor,
        currentPeelRegion());
}

// =============================================================================
// Internal helpers
// =============================================================================

CTImage* MainWindow::currentImage()
{
    if (m_images.empty() || m_currentIndex < 0
        || m_currentIndex >= static_cast<int>(m_images.size()))
        return nullptr;
    return m_images[m_currentIndex].get();
}

double MainWindow::currentBeta() const
{
    return m_ui->doubleSpinBox_DetectionProbe->value();
}

// See the header comment on runEngineTask() for the full concurrency
// contract. Summary: runs `work` on a detached std::thread, then marshals
// back to the GUI thread (Qt::QueuedConnection) to invoke
// `onDone(success, errorMessage)`. m_calcRunning is the single flag that
// serializes ALL engine-touching background tasks in this window (shell
// calculation, peeling, DICOM load) because the underlying V-ImageProcessing
// engine is not thread-safe and may hold process-wide state.
void MainWindow::runEngineTask(
    std::function<void()> work,
    std::function<void(bool success, const std::string& errMsg)> onDone)
{
    if (m_calcRunning) {
        // Should never happen if call sites check m_calcRunning first
        // (they do); fail safe rather than racing two engine calls.
        return;
    }
    m_calcRunning = true;

    std::thread([this, work = std::move(work), onDone = std::move(onDone)]() mutable {
        bool ok = false;
        std::string errMsg;
        try {
            work();
            ok = true;
        } catch (const std::exception& e) {
            errMsg = e.what();
        } catch (...) {
            errMsg = "Unknown error during background engine operation";
        }

        QMetaObject::invokeMethod(this,
            [this, onDone = std::move(onDone), ok, errMsg]() mutable {
                // Reset BEFORE onDone() so that onDone() (or anything it
                // triggers, e.g. a chained navigation) can start a new task.
                m_calcRunning = false;

                onDone(ok, errMsg);

                // Replay the latest slice navigation request that arrived
                // while this task was running (see navigateToImage()).
                if (m_pendingNavIndex >= 0) {
                    int next = m_pendingNavIndex;
                    m_pendingNavIndex = -1;
                    navigateToImage(next);
                }
            },
            Qt::QueuedConnection);
    }).detach();
}

PeelRegion MainWindow::currentPeelRegion() const
{
    if (m_ui->radioButton_UpperHalf->isChecked()) return PeelRegion::UpperHalf;
    if (m_ui->radioButton_LowerHalf->isChecked()) return PeelRegion::LowerHalf;
    if (m_ui->radioButton_LeftHalf->isChecked())  return PeelRegion::LeftHalf;
    if (m_ui->radioButton_RightHalf->isChecked()) return PeelRegion::RightHalf;
    return PeelRegion::WholeBody;
}

// =============================================================================
// Annotation (expert segmentation)
// =============================================================================

void MainWindow::onAnnotationToggle(bool checked)
{
    // Defensive — see runEngineTask()'s concurrency contract in MainWindow.h.
    if (m_calcRunning) return;

    m_ui->graphicsView->setAnnotationMode(checked);
    if (checked) {
        m_ui->graphicsView->setCursor(Qt::CrossCursor);
        m_scene->initAnnotation();
        statusBar()->showMessage("Annotation mode: click/drag to paint skin pixels. Right-click to pan.");
    } else {
        m_ui->graphicsView->setCursor(Qt::ArrowCursor);
        statusBar()->clearMessage();
    }
}

void MainWindow::onAnnotationPaint(QPointF scenePos)
{
    // Defensive — see runEngineTask()'s concurrency contract in MainWindow.h.
    if (m_calcRunning) return;

    int col = static_cast<int>(scenePos.x());
    int row = static_cast<int>(scenePos.y());
    m_scene->toggleAnnotation(col, row);
}

void MainWindow::onAnnotationClear()
{
    // Defensive — see runEngineTask()'s concurrency contract in MainWindow.h.
    if (m_calcRunning) return;

    m_scene->clearAnnotation();
    // Redisplay current CT to remove overlay
    CTImage* img = currentImage();
    if (img) m_scene->displayCTImage(*img);
    statusBar()->showMessage("Annotation cleared.", 2000);
}

void MainWindow::onAnnotationExport()
{
    // Defensive — see runEngineTask()'s concurrency contract in MainWindow.h.
    if (m_calcRunning) return;

    CTImage* img = currentImage();
    if (!img) { QMessageBox::warning(this, "Export", "No image loaded."); return; }
    if (!m_scene->hasAnnotation()) { QMessageBox::warning(this, "Export", "No annotation to export."); return; }

    QString dir = QFileDialog::getExistingDirectory(this, "Select export folder");
    if (dir.isEmpty()) return;

    // Build filename from slice info
    QString baseName = QFileInfo(QString::fromStdString(img->filepath())).completeBaseName();

    // 1. Annotated PNG (CT + yellow)
    QImage annotated = m_scene->exportAnnotatedImage(*img);
    QString annPath = dir + "/" + baseName + "_annotated.png";
    annotated.save(annPath, "PNG");

    // 2. Binary mask
    QImage mask = m_scene->exportMaskImage();
    QString maskPath = dir + "/" + baseName + "_mask.png";
    mask.save(maskPath, "PNG");

    QMessageBox::information(this, "Export Complete",
        QString("Saved:\n%1\n%2").arg(annPath, maskPath));
}

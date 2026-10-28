#include "PlotWindow.h"
#include "PlotWidgets.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QStyle>
#include <QPixmap>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFrame>
#include <QTimer>

PlotWindow::PlotWindow(PlotWidget* plotWidget, const QString& title, QWidget* parent)
    : QDialog(parent), m_plot(plotWidget) {
    setWindowTitle(title);
    setMinimumSize(1050, 680);
    resize(1150, 800);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Plot area (stretch)
    mainLayout->addWidget(m_plot, 1);

    // Add an axis-range control panel when the plot is a GaussianPlotWidget
    // or ScatterPlotWidget
    bool isGaussian = qobject_cast<GaussianPlotWidget*>(m_plot) != nullptr;
    bool isScatter = qobject_cast<ScatterPlotWidget*>(m_plot) != nullptr;
    if (isGaussian || isScatter) {
        auto* ctrlWidget = new QWidget(this);
        ctrlWidget->setMaximumHeight(40);
        auto* ctrlLayout = new QHBoxLayout(ctrlWidget);
        ctrlLayout->setContentsMargins(8, 4, 8, 4);
        ctrlLayout->setSpacing(6);

        auto makeAxisSpin = [&](QWidget* parent) -> QDoubleSpinBox* {
            auto* sb = new QDoubleSpinBox(parent);
            sb->setRange(-99999.0, 99999.0);
            sb->setDecimals(3);
            sb->setSingleStep(1.0);
            sb->setFixedWidth(90);
            return sb;
        };

        ctrlLayout->addWidget(new QLabel("X min:"));
        auto* xMinSpin = makeAxisSpin(ctrlWidget);
        ctrlLayout->addWidget(xMinSpin);

        ctrlLayout->addWidget(new QLabel("X max:"));
        auto* xMaxSpin = makeAxisSpin(ctrlWidget);
        ctrlLayout->addWidget(xMaxSpin);

        auto* sep = new QFrame(ctrlWidget);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Sunken);
        ctrlLayout->addWidget(sep);

        ctrlLayout->addWidget(new QLabel("Y min:"));
        auto* yMinSpin = makeAxisSpin(ctrlWidget);
        ctrlLayout->addWidget(yMinSpin);

        ctrlLayout->addWidget(new QLabel("Y max:"));
        auto* yMaxSpin = makeAxisSpin(ctrlWidget);
        ctrlLayout->addWidget(yMaxSpin);

        // Reset button
        auto* sep2 = new QFrame(ctrlWidget);
        sep2->setFrameShape(QFrame::VLine);
        sep2->setFrameShadow(QFrame::Sunken);
        ctrlLayout->addWidget(sep2);

        auto* resetBtn = new QPushButton("Reset", ctrlWidget);
        resetBtn->setFixedWidth(56);
        ctrlLayout->addWidget(resetBtn);

        ctrlLayout->addStretch();
        mainLayout->addWidget(ctrlWidget);

        // Spinbox value changed → apply range
        PlotWidget* pw = m_plot;
        auto applyX = [pw, xMinSpin, xMaxSpin]() {
            pw->setXRange(xMinSpin->value(), xMaxSpin->value());
        };
        auto applyY = [pw, yMinSpin, yMaxSpin]() {
            pw->setYRange(yMinSpin->value(), yMaxSpin->value());
        };
        connect(xMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), ctrlWidget,
                [applyX](double) { applyX(); });
        connect(xMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), ctrlWidget,
                [applyX](double) { applyX(); });
        connect(yMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), ctrlWidget,
                [applyY](double) { applyY(); });
        connect(yMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), ctrlWidget,
                [applyY](double) { applyY(); });
        connect(resetBtn, &QPushButton::clicked, pw, &PlotWidget::resetAxisRange);

        // After first paint: initialize spinboxes with auto-computed range
        connect(resetBtn, &QPushButton::clicked, ctrlWidget,
                [pw, xMinSpin, xMaxSpin, yMinSpin, yMaxSpin]() {
                    // When reset is clicked, also update spinboxes to reflect auto range
                    // (Need to defer by one event loop tick after update() call in resetAxisRange)
                    QTimer::singleShot(0, [pw, xMinSpin, xMaxSpin, yMinSpin, yMaxSpin]() {
                        auto blockAll = [](QWidget* w, bool b) { w->blockSignals(b); };
                        blockAll(xMinSpin, true);
                        blockAll(xMaxSpin, true);
                        blockAll(yMinSpin, true);
                        blockAll(yMaxSpin, true);
                        xMinSpin->setValue(pw->lastXMin());
                        xMaxSpin->setValue(pw->lastXMax());
                        yMinSpin->setValue(pw->lastYMin());
                        yMaxSpin->setValue(pw->lastYMax());
                        blockAll(xMinSpin, false);
                        blockAll(xMaxSpin, false);
                        blockAll(yMinSpin, false);
                        blockAll(yMaxSpin, false);
                    });
                });

        // Initialize spinboxes after first paint (50ms delay to ensure paint has occurred)
        QTimer::singleShot(50, ctrlWidget, [pw, xMinSpin, xMaxSpin, yMinSpin, yMaxSpin]() {
            auto blockAll = [](QWidget* w, bool b) { w->blockSignals(b); };
            blockAll(xMinSpin, true);
            blockAll(xMaxSpin, true);
            blockAll(yMinSpin, true);
            blockAll(yMaxSpin, true);
            xMinSpin->setValue(pw->lastXMin());
            xMaxSpin->setValue(pw->lastXMax());
            yMinSpin->setValue(pw->lastYMin());
            yMaxSpin->setValue(pw->lastYMax());
            blockAll(xMinSpin, false);
            blockAll(xMaxSpin, false);
            blockAll(yMinSpin, false);
            blockAll(yMaxSpin, false);
        });
    }

    // Save button row
    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_saveBtn = new QPushButton(this);
    m_saveBtn->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveBtn->setText("Save as PNG");
    m_saveBtn->setFixedHeight(28);
    btnLayout->addWidget(m_saveBtn);

    mainLayout->addLayout(btnLayout);

    connect(m_saveBtn, &QPushButton::clicked, this, &PlotWindow::onSave);
}

void PlotWindow::onSave() {
    if (!m_plot)
        return;
    QString filename = QFileDialog::getSaveFileName(this, "Save Plot", QString(),
                                                    "PNG Images (*.png);;All Files (*)");
    if (filename.isEmpty())
        return;

    // Ensure extension
    if (!filename.endsWith(".png", Qt::CaseInsensitive))
        filename += ".png";

    QPixmap pix = m_plot->grab();
    pix.save(filename, "PNG");
}

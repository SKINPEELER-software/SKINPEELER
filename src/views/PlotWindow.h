#pragma once
#include <QDialog>
#include <QString>

class PlotWidget;
class QPushButton;

// =============================================================================
// PlotWindow — Standalone window wrapping a single PlotWidget with a Save button
// =============================================================================
class PlotWindow : public QDialog {
    Q_OBJECT
public:
    explicit PlotWindow(PlotWidget* plotWidget, const QString& title, QWidget* parent = nullptr);

private slots:
    void onSave();

private:
    PlotWidget* m_plot = nullptr;
    QPushButton* m_saveBtn = nullptr;
};

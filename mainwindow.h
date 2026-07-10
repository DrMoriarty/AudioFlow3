#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSpinBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QLabel>
#include <QComboBox>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CollapsibleBlock;
class QWidget;
class Config;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const Config &config, QWidget *parent = nullptr);
    ~MainWindow() override;
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupBlocks();
    void updateFixedHeight();
    static void updateSliderColor(QSlider *slider);

    Ui::MainWindow *ui;
    const Config &m_config;
    QList<CollapsibleBlock *> m_blocks;
    QWidget *m_eqContent = nullptr;
    int m_eqContentHeight = 0;
    int m_blockCollapsedHeight = 0;
    QVector<int> m_expandedHeights;
    bool m_initialized = false;
    int m_titleBarHeight = -1;
    QTimer *m_ampAutoTimer = nullptr;
    QLabel *m_latencyLabel = nullptr;
    QLabel *m_processLabel = nullptr;
    QWidget *m_vuL = nullptr;
    QWidget *m_vuR = nullptr;

    static const int BAND_COUNT = 10;
    QSpinBox *m_eqHz[BAND_COUNT] = {};
    QSlider *m_eqGain[BAND_COUNT] = {};
    QSpinBox *m_eqGainSpin[BAND_COUNT] = {};
    QDoubleSpinBox *m_eqQ[BAND_COUNT] = {};
    QComboBox *m_eqPresetCombo = nullptr;
    bool m_eqPresetLoading = false;
};
#endif // MAINWINDOW_H

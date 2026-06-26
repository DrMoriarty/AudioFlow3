#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupBlocks();
    void updateFixedHeight();

    Ui::MainWindow *ui;
    const Config &m_config;
    QList<CollapsibleBlock *> m_blocks;
    QWidget *m_eqContent = nullptr;
    int m_eqContentHeight = 0;
    int m_blockCollapsedHeight = 0;
    QVector<int> m_expandedHeights;
    bool m_initialized = false;
    int m_titleBarHeight = -1;
};
#endif // MAINWINDOW_H

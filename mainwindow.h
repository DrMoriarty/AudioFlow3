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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupBlocks();
    void updateFixedHeight();

    Ui::MainWindow *ui;
    QList<CollapsibleBlock *> m_blocks;
    QWidget *m_eqContent = nullptr;
    bool m_initialized = false;
};
#endif // MAINWINDOW_H

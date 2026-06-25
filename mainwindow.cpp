#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "collapsibleblock.h"

#include <QVBoxLayout>
#include <QShowEvent>
#include <QTimer>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupBlocks();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!m_initialized) {
        m_initialized = true;
        updateFixedHeight();
    }
}

void MainWindow::updateFixedHeight()
{
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    centralWidget()->layout()->invalidate();
    QApplication::processEvents();
    adjustSize();
    setFixedSize(size());
}

void MainWindow::setupBlocks()
{
    setFixedWidth(600);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(5);

    QStringList blockTitles = {
        tr("Блок 1"),
        tr("Блок 2"),
        tr("Блок 3"),
        tr("Блок 4"),
        tr("Блок 5")
    };

    for (const QString &title : blockTitles) {
        CollapsibleBlock *block = new CollapsibleBlock(title, centralWidget);
        connect(block, &CollapsibleBlock::expandedChanged, this, &MainWindow::updateFixedHeight);
        mainLayout->addWidget(block);
        m_blocks.append(block);
    }

    setCentralWidget(centralWidget);
}

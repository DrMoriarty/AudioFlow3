#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "collapsibleblock.h"
#include "knobwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
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

    for (auto *block : m_blocks) {
        block->setMinimumHeight(0);
        block->setMaximumHeight(QWIDGETSIZE_MAX);
    }

    centralWidget()->layout()->invalidate();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    adjustSize();
    setFixedSize(size());
}

void MainWindow::setupBlocks()
{
    setFixedWidth(600);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(5);

    QStringList blockTitles = {
        tr("Correcting"),
        tr("Блок 2"),
        tr("Блок 3"),
        tr("Блок 4"),
        tr("Блок 5")
    };

    for (int i = 0; i < blockTitles.size(); ++i) {
        CollapsibleBlock *block = new CollapsibleBlock(blockTitles[i], centralWidget);
        connect(block, &CollapsibleBlock::expandedChanged, this, &MainWindow::updateFixedHeight);
        mainLayout->addWidget(block);
        m_blocks.append(block);
    }

    QWidget *correctingContent = new QWidget();
    QVBoxLayout *ccLayout = new QVBoxLayout(correctingContent);
    ccLayout->setContentsMargins(8, 4, 8, 4);
    ccLayout->setSpacing(4);

    QLabel *descLabel = new QLabel(tr("Correcting the frequency response of your audio device"));
    descLabel->setWordWrap(true);
    ccLayout->addWidget(descLabel);

    QHBoxLayout *irLayout = new QHBoxLayout();
    QComboBox *irCombo = new QComboBox();
    irCombo->addItem(tr("Flat"));
    irLayout->addWidget(irCombo, 1);
    QPushButton *loadIrBtn = new QPushButton(tr("Load IR"));
    irLayout->addWidget(loadIrBtn);
    ccLayout->addLayout(irLayout);

    QVBoxLayout *sliderColumn = new QVBoxLayout();
    sliderColumn->setContentsMargins(0, 0, 0, 0);
    sliderColumn->setSpacing(4);
    QHBoxLayout *mixHeaderLayout = new QHBoxLayout();
    mixHeaderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *mixLabel = new QLabel(tr("Dry/Wet Mix"));
    mixHeaderLayout->addWidget(mixLabel);
    mixHeaderLayout->addSpacing(10);
    QLabel *mixValueLabel = new QLabel("50%");
    mixHeaderLayout->addWidget(mixValueLabel);
    mixHeaderLayout->addStretch();
    sliderColumn->addLayout(mixHeaderLayout);
    QSlider *mixSlider = new QSlider(Qt::Horizontal);
    mixSlider->setRange(0, 100);
    mixSlider->setValue(50);
    connect(mixSlider, &QSlider::valueChanged, this, [mixValueLabel](int v) {
        mixValueLabel->setText(QString::number(v) + "%");
    });
    sliderColumn->addWidget(mixSlider);

    QHBoxLayout *mixKnobLayout = new QHBoxLayout();
    mixKnobLayout->setContentsMargins(0, 0, 0, 0);
    mixKnobLayout->setSpacing(8);
    mixKnobLayout->addLayout(sliderColumn, 1);

    QVBoxLayout *knobLayout = new QVBoxLayout();
    knobLayout->setContentsMargins(0, 0, 0, 0);
    knobLayout->setSpacing(2);
    knobLayout->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    QLabel *gainLabel = new QLabel(tr("Gain"));
    gainLabel->setAlignment(Qt::AlignCenter);
    knobLayout->addWidget(gainLabel);
    KnobWidget *gainKnob = new KnobWidget({"0 dB", "3 dB", "6 dB", "9 dB", "12 dB"});
    knobLayout->addWidget(gainKnob);
    mixKnobLayout->addLayout(knobLayout);
    ccLayout->addLayout(mixKnobLayout);

    m_blocks[0]->setContentWidget(correctingContent);
    setCentralWidget(centralWidget);
    m_blocks[0]->setExpanded(true);
}

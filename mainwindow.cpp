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
#include <QCheckBox>
#include <QShowEvent>
#include <QTimer>
#include <QApplication>
#include <QPalette>

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
        tr("Preamplifier"),
        tr("Блок 3"),
        tr("Convolver"),
        tr("Settings")
    };

    QPalette pal = QApplication::palette();
    pal.setColor(QPalette::Inactive, QPalette::Text, pal.color(QPalette::Active, QPalette::Text));
    pal.setColor(QPalette::Inactive, QPalette::ButtonText, pal.color(QPalette::Active, QPalette::ButtonText));
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, pal.color(QPalette::Active, QPalette::HighlightedText));
    QApplication::setPalette(pal);

    centralWidget->setStyleSheet(
        "QComboBox { color: #e0e0e0; }"
        "QComboBox QAbstractItemView { color: #e0e0e0; }"
    );

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

    QWidget *preampContent = new QWidget();
    QVBoxLayout *paLayout = new QVBoxLayout(preampContent);
    paLayout->setContentsMargins(8, 4, 8, 4);
    paLayout->setSpacing(4);

    QHBoxLayout *gainRowLayout = new QHBoxLayout();
    gainRowLayout->setContentsMargins(0, 0, 0, 0);
    gainRowLayout->setSpacing(8);
    QLabel *paGainLabel = new QLabel(tr("Gain"));
    gainRowLayout->addWidget(paGainLabel);
    QLabel *gainValueLabel = new QLabel("+0 dB");
    gainRowLayout->addWidget(gainValueLabel);
    gainRowLayout->addStretch();
    QCheckBox *autoSwitch = new QCheckBox(tr("Auto"));
    gainRowLayout->addWidget(autoSwitch);
    paLayout->addLayout(gainRowLayout);

    QSlider *gainSlider = new QSlider(Qt::Horizontal);
    gainSlider->setRange(-30, 30);
    gainSlider->setValue(0);
    connect(gainSlider, &QSlider::valueChanged, this, [gainValueLabel](int v) {
        gainValueLabel->setText(QString("%1%2 dB").arg(v >= 0 ? "+" : "").arg(v));
    });
    paLayout->addWidget(gainSlider);

    QHBoxLayout *sliderLabelLayout = new QHBoxLayout();
    sliderLabelLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *minLabel = new QLabel(tr("-30 dB"));
    sliderLabelLayout->addWidget(minLabel);
    sliderLabelLayout->addStretch();
    QLabel *zeroLabel = new QLabel(tr("0 dB"));
    sliderLabelLayout->addWidget(zeroLabel);
    sliderLabelLayout->addStretch();
    QLabel *maxLabel = new QLabel(tr("+30 dB"));
    sliderLabelLayout->addWidget(maxLabel);
    paLayout->addLayout(sliderLabelLayout);

    m_blocks[1]->setContentWidget(preampContent);

    QWidget *convolverContent = new QWidget();
    QVBoxLayout *cvLayout = new QVBoxLayout(convolverContent);
    cvLayout->setContentsMargins(8, 4, 8, 4);
    cvLayout->setSpacing(4);

    QLabel *spaceLabel = new QLabel(tr("Space"));
    cvLayout->addWidget(spaceLabel);

    QHBoxLayout *spaceRowLayout = new QHBoxLayout();
    spaceRowLayout->setContentsMargins(0, 0, 0, 0);
    spaceRowLayout->setSpacing(8);
    QComboBox *spaceCombo = new QComboBox();
    spaceCombo->addItem(tr("Flat"));
    spaceRowLayout->addWidget(spaceCombo, 1);
    QPushButton *customBtn = new QPushButton(tr("Custom"));
    spaceRowLayout->addWidget(customBtn);
    cvLayout->addLayout(spaceRowLayout);

    QHBoxLayout *wetMixHeader = new QHBoxLayout();
    wetMixHeader->setContentsMargins(0, 0, 0, 0);
    wetMixHeader->setSpacing(8);
    QLabel *cvMixLabel = new QLabel(tr("Dry/Wet Mix"));
    wetMixHeader->addWidget(cvMixLabel);
    QLabel *cvMixValue = new QLabel("50%");
    wetMixHeader->addWidget(cvMixValue);
    wetMixHeader->addStretch();
    cvLayout->addLayout(wetMixHeader);

    QSlider *cvMixSlider = new QSlider(Qt::Horizontal);
    cvMixSlider->setRange(0, 100);
    cvMixSlider->setValue(50);
    connect(cvMixSlider, &QSlider::valueChanged, this, [cvMixValue](int v) {
        cvMixValue->setText(QString::number(v) + "%");
    });
    cvLayout->addWidget(cvMixSlider);

    m_blocks[3]->setContentWidget(convolverContent);

    QWidget *settingsContent = new QWidget();
    QVBoxLayout *stLayout = new QVBoxLayout(settingsContent);
    stLayout->setContentsMargins(8, 4, 8, 4);
    stLayout->setSpacing(4);

    QLabel *outDevLabel = new QLabel(tr("Output device"));
    stLayout->addWidget(outDevLabel);
    QComboBox *outDevCombo = new QComboBox();
    stLayout->addWidget(outDevCombo);

    QLabel *bufSizeLabel = new QLabel(tr("Buffer size"));
    stLayout->addWidget(bufSizeLabel);
    QComboBox *bufSizeCombo = new QComboBox();
    bufSizeCombo->addItem("64");
    bufSizeCombo->addItem("128");
    bufSizeCombo->addItem("256");
    bufSizeCombo->addItem("512");
    bufSizeCombo->addItem("1024");
    bufSizeCombo->addItem("2048");
    stLayout->addWidget(bufSizeCombo);

    m_blocks[4]->setContentWidget(settingsContent);

    setCentralWidget(centralWidget);
    m_blocks[0]->setExpanded(true);
}

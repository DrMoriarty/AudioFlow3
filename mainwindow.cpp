#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "collapsibleblock.h"
#include "knobwidget.h"
#include "src/fileutils/config.h"
#include "src/audioflow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QShowEvent>
#include <QFileInfo>
#include <cmath>
#include <QTimer>
#include <QApplication>
#include <QPalette>

MainWindow::MainWindow(const Config &config, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_config(config)
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

        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        for (int i = 0; i < m_blocks.size(); ++i) {
            if (i == 2 && m_eqContentHeight > 0)
                m_expandedHeights[i] = m_blocks[i]->headerHeight() + m_eqContentHeight;
            else
                m_expandedHeights[i] = m_blocks[i]->headerHeight() + m_blocks[i]->contentWidget()->sizeHint().height();
        }

        updateFixedHeight();
    }
}

void MainWindow::updateFixedHeight()
{
    QLayout *cl = centralWidget()->layout();
    int totalH = cl->contentsMargins().top() + cl->contentsMargins().bottom();
    for (int i = 0; i < m_blocks.size(); ++i) {
        CollapsibleBlock *block = m_blocks[i];
        int blockH;
        if (block == m_blocks[2] && block->isExpanded() && m_eqContentHeight > 0)
            blockH = m_blocks[2]->headerHeight() + m_eqContentHeight;
        else if (block->isExpanded() && m_expandedHeights[i] > 0)
            blockH = m_expandedHeights[i];
        else if (block->isExpanded())
            blockH = m_blockCollapsedHeight + 100;
        else
            blockH = m_blockCollapsedHeight;
        block->setFixedHeight(blockH);
        totalH += blockH;
    }
    totalH += cl->spacing() * (m_blocks.size() - 1);

    if (m_titleBarHeight <= 0)
        m_titleBarHeight = frameGeometry().height() - geometry().height();
    if (m_titleBarHeight <= 0)
        m_titleBarHeight = 28;

    setFixedSize(600, totalH + m_titleBarHeight);
}

void MainWindow::setupBlocks()
{
    setFixedWidth(600);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(1);

    QStringList blockTitles = {
        tr("Correcting"),
        tr("Preamplifier"),
        tr("Equalizer"),
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
        if (i < 4)
            block->addToggleSwitch();
    }
    m_blocks[0]->setToggleChecked(m_config.correctionToggle);
    m_blocks[1]->setToggleChecked(m_config.ampToggle);
    m_blocks[2]->setToggleChecked(m_config.equalizerToggle);
    m_blocks[3]->setToggleChecked(m_config.reverbToggle);
    m_blockCollapsedHeight = m_blocks[0]->headerHeight();
    m_expandedHeights.resize(m_blocks.size(), -1);

    QWidget *correctingContent = new QWidget();
    QVBoxLayout *ccLayout = new QVBoxLayout(correctingContent);
    ccLayout->setContentsMargins(8, 4, 8, 4);
    ccLayout->setSpacing(4);

    QLabel *descLabel = new QLabel(tr("Correcting the frequency response of your audio device"));
    descLabel->setWordWrap(true);
    ccLayout->addWidget(descLabel);

    QHBoxLayout *irLayout = new QHBoxLayout();
    QComboBox *irCombo = new QComboBox();
    QString corrIRName = QFileInfo(QString::fromStdString(m_config.correctionIRFilePath)).fileName();
    int corrComboIndex = -1;
    for (const auto &path : m_config.correctionRecent) {
        QString fname = QFileInfo(QString::fromStdString(path)).fileName();
        irCombo->addItem(fname, QString::fromStdString(path));
        if (fname == corrIRName)
            corrComboIndex = irCombo->count() - 1;
    }
    if (corrComboIndex >= 0)
        irCombo->setCurrentIndex(corrComboIndex);
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
    mixSlider->setValue(static_cast<int>(m_config.correctionDryWet * 100));
    connect(mixSlider, &QSlider::valueChanged, this, [mixValueLabel](int v) {
        mixValueLabel->setText(QString::number(v) + "%");
    });
    mixValueLabel->setText(QString::number(mixSlider->value()) + "%");
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
    float bestDist = 1000.0f;
    int bestIdx = 0;
    const float knobValues[] = {0.0f, 3.0f, 6.0f, 9.0f, 12.0f};
    for (int k = 0; k < 5; ++k) {
        float dist = fabsf(m_config.correctionPostGain - knobValues[k]);
        if (dist < bestDist) { bestDist = dist; bestIdx = k; }
    }
    gainKnob->setCurrentIndex(bestIdx);
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
    gainSlider->setValue(static_cast<int>(m_config.ampGain));
    connect(gainSlider, &QSlider::valueChanged, this, [gainValueLabel](int v) {
        gainValueLabel->setText(QString("%1%2 dB").arg(v >= 0 ? "+" : "").arg(v));
    });
    gainValueLabel->setText(QString("%1%2 dB").arg(gainSlider->value() >= 0 ? "+" : "").arg(gainSlider->value()));
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

    QWidget *equalizerContent = new QWidget();
    m_eqContent = equalizerContent;
    QVBoxLayout *eqLayout = new QVBoxLayout(equalizerContent);
    eqLayout->setContentsMargins(8, 4, 8, 4);
    eqLayout->setSpacing(4);

    QLabel *presetLabel = new QLabel(tr("Preset"));
    eqLayout->addWidget(presetLabel);
    QComboBox *presetCombo = new QComboBox();
    eqLayout->addWidget(presetCombo);

    QLabel *parametersLabel = new QLabel(tr("Parameters"));
    eqLayout->addWidget(parametersLabel);

    QGridLayout *eqGrid = new QGridLayout();
    eqGrid->setContentsMargins(0, 0, 0, 2);
    eqGrid->setSpacing(2);

    QLabel *hzLabel = new QLabel("Hz");
    eqGrid->addWidget(hzLabel, 0, 0);

    QWidget *eqScaleLabels = new QWidget();
    QVBoxLayout *eqScaleLayout = new QVBoxLayout(eqScaleLabels);
    eqScaleLayout->setContentsMargins(0, 0, 0, 0);
    eqScaleLayout->setSpacing(0);

    QLabel *eqPlusLabel = new QLabel("+30 dB");
    eqPlusLabel->setAlignment(Qt::AlignCenter);
    QLabel *eqZeroLabel = new QLabel("0 dB");
    eqZeroLabel->setAlignment(Qt::AlignCenter);
    QLabel *eqMinusLabel = new QLabel("-30 dB");
    eqMinusLabel->setAlignment(Qt::AlignCenter);
    eqScaleLayout->addWidget(eqPlusLabel, 0, Qt::AlignTop | Qt::AlignHCenter);
    eqScaleLayout->addStretch(1);
    eqScaleLayout->addWidget(eqZeroLabel, 0, Qt::AlignVCenter | Qt::AlignHCenter);
    eqScaleLayout->addStretch(1);
    eqScaleLayout->addWidget(eqMinusLabel, 0, Qt::AlignBottom | Qt::AlignHCenter);
    eqGrid->addWidget(eqScaleLabels, 1, 0, Qt::AlignHCenter);

    const int bandCount = 10;
    QSpinBox *hzSpinBoxes[bandCount];
    QSlider *eqSliders[bandCount];
    QSpinBox *gainSpinboxes[bandCount];
    QDoubleSpinBox *qSpinboxes[bandCount];

    int defaultHz[] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

    for (int i = 0; i < bandCount; ++i) {
        int col = i + 1;

        hzSpinBoxes[i] = new QSpinBox();
        hzSpinBoxes[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        hzSpinBoxes[i]->setAlignment(Qt::AlignCenter);
        hzSpinBoxes[i]->setRange(10, 20000);
        hzSpinBoxes[i]->setValue(i < m_config.equalizerF.size() ? static_cast<int>(m_config.equalizerF[i]) : defaultHz[i]);
        eqGrid->addWidget(hzSpinBoxes[i], 0, col);

        eqSliders[i] = new QSlider(Qt::Vertical);
        eqSliders[i]->setRange(-30, 30);
        eqSliders[i]->setValue(i < m_config.equalizerG.size() ? static_cast<int>(m_config.equalizerG[i]) : 0);
        eqSliders[i]->setFixedHeight(200);
        eqGrid->addWidget(eqSliders[i], 1, col, Qt::AlignHCenter);

        gainSpinboxes[i] = new QSpinBox();
        gainSpinboxes[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        gainSpinboxes[i]->setAlignment(Qt::AlignCenter);
        gainSpinboxes[i]->setRange(-30, 30);
        gainSpinboxes[i]->setValue(i < m_config.equalizerG.size() ? static_cast<int>(m_config.equalizerG[i]) : 0);
        eqGrid->addWidget(gainSpinboxes[i], 2, col);

        connect(eqSliders[i], &QSlider::valueChanged, gainSpinboxes[i], &QSpinBox::setValue);
        connect(gainSpinboxes[i], QOverload<int>::of(&QSpinBox::valueChanged), eqSliders[i], &QSlider::setValue);

        qSpinboxes[i] = new QDoubleSpinBox();
        qSpinboxes[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        qSpinboxes[i]->setAlignment(Qt::AlignCenter);
        qSpinboxes[i]->setRange(0.1, 15.0);
        qSpinboxes[i]->setValue(i < m_config.equalizerQ.size() ? static_cast<double>(m_config.equalizerQ[i]) : 1.0);
        qSpinboxes[i]->setSingleStep(0.1);
        eqGrid->addWidget(qSpinboxes[i], 3, col);
    }

    QLabel *gainLabel2 = new QLabel("Gain");
    eqGrid->addWidget(gainLabel2, 2, 0);

    QLabel *qLabel = new QLabel("Q value");
    eqGrid->addWidget(qLabel, 3, 0);

    eqLayout->addLayout(eqGrid);

    int eqContentH = eqLayout->contentsMargins().top() + eqLayout->contentsMargins().bottom();
    eqContentH += presetLabel->sizeHint().height() + presetCombo->sizeHint().height();
    eqContentH += parametersLabel->sizeHint().height();
    eqContentH += eqLayout->spacing() * 3;
    int sliderH = 200; // fixed slider height
    int spinH = hzSpinBoxes[0]->sizeHint().height();
    int gainH = gainSpinboxes[0]->sizeHint().height();
    int qH = qSpinboxes[0]->sizeHint().height();
    eqContentH += spinH + eqGrid->spacing() + sliderH + eqGrid->spacing() + gainH + eqGrid->spacing() + qH;
    eqContentH += eqGrid->contentsMargins().top() + eqGrid->contentsMargins().bottom();
    equalizerContent->setMinimumHeight(eqContentH);
    m_eqContentHeight = eqContentH;

    m_blocks[2]->setContentWidget(equalizerContent);

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
    if (!m_config.irFilePath.empty()) {
        spaceCombo->addItem(QFileInfo(QString::fromStdString(m_config.irFilePath)).fileName(),
                             QString::fromStdString(m_config.irFilePath));
    } else {
        spaceCombo->addItem(tr("Flat"));
    }
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
    cvMixSlider->setValue(static_cast<int>(m_config.reverbDryWet * 100));
    connect(cvMixSlider, &QSlider::valueChanged, this, [cvMixValue](int v) {
        cvMixValue->setText(QString::number(v) + "%");
    });
    cvMixValue->setText(QString::number(cvMixSlider->value()) + "%");
    cvLayout->addWidget(cvMixSlider);

    m_blocks[3]->setContentWidget(convolverContent);

    QWidget *settingsContent = new QWidget();
    QVBoxLayout *stLayout = new QVBoxLayout(settingsContent);
    stLayout->setContentsMargins(8, 4, 8, 4);
    stLayout->setSpacing(4);

    QLabel *outDevLabel = new QLabel(tr("Output device"));
    stLayout->addWidget(outDevLabel);
    QComboBox *outDevCombo = new QComboBox();
    QStringList outDevNames;
    for (const auto &name : getAvailableOutputDevices())
        outDevNames.append(QString::fromStdString(name));
    outDevCombo->addItems(outDevNames);
    int outDevIdx = outDevNames.indexOf(QString::fromStdString(getCurrentOutputDeviceName()));
    if (outDevIdx >= 0)
        outDevCombo->setCurrentIndex(outDevIdx);
    stLayout->addWidget(outDevCombo);

    QLabel *bufSizeLabel = new QLabel(tr("Buffer size"));
    stLayout->addWidget(bufSizeLabel);
    QComboBox *bufSizeCombo = new QComboBox();
    const int bufSizes[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    int bufIdx = -1;
    for (int bs : bufSizes) {
        bufSizeCombo->addItem(QString::number(bs), bs);
        if (bs == m_config.bufferSize)
            bufIdx = bufSizeCombo->count() - 1;
    }
    if (bufIdx >= 0)
        bufSizeCombo->setCurrentIndex(bufIdx);
    stLayout->addWidget(bufSizeCombo);

    m_blocks[4]->setContentWidget(settingsContent);

    setCentralWidget(centralWidget);
    m_blocks[0]->setExpanded(m_config.uiExpandedCorrecting);
    m_blocks[1]->setExpanded(m_config.uiExpandedPreamplifier);
    m_blocks[2]->setExpanded(m_config.uiExpandedEqualizer);
    m_blocks[3]->setExpanded(m_config.uiExpandedReverb);
    m_blocks[4]->setExpanded(m_config.uiExpandedSettings);
}

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
#include <QDir>
#include <QCoreApplication>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <QTimer>
#include <QApplication>
#include <QPalette>
#include <QColor>

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

void MainWindow::updateSliderColor(QSlider *slider)
{
    double t = 0.0;
    int range = slider->maximum() - slider->minimum();
    if (range > 0)
        t = static_cast<double>(slider->value() - slider->minimum()) / range;

    int h = static_cast<int>(240 + t * 120);
    if (h >= 360) h -= 360;
    QColor color = QColor::fromHsv(h, 200, 40 + 120 * t);
    QString c = color.name();

    bool horiz = slider->orientation() == Qt::Horizontal;
    if (horiz) {
        slider->setStyleSheet(QStringLiteral(
            "QSlider { padding: 7px; }"
            "QSlider::groove:horizontal { height: 14px; background: #3a3a3a; border: 1px solid #606468; border-radius: 3px; }"
            "QSlider::sub-page:horizontal { background: %1; border: 1px solid #606468; border-radius: 3px; }"
            "QSlider::add-page:horizontal { background: #3a3a3a; border: 1px solid #606468; border-radius: 3px; }"
            "QSlider::handle:horizontal { width: 14px; margin: -1px 0; background: #a0a0a0; border: 1px solid #808080; border-radius: 3px; }"
        ).arg(c));
    } else {
        slider->setStyleSheet(QStringLiteral(
            "QSlider { padding: 7px; }"
            "QSlider::groove:vertical { width: 14px; background: #3a3a3a; border: 1px solid #606468; border-radius: 3px; }"
            "QSlider::sub-page:vertical { background: #3a3a3a; border: 1px solid #606468; border-radius: 3px; }"
            "QSlider::add-page:vertical { background: %1; border: 1px solid #606468; border-radius: 3px; }"
            "QSlider::handle:vertical { height: 14px; margin: 0 -1px; background: #a0a0a0; border: 1px solid #808080; border-radius: 3px; }"
        ).arg(c));
    }
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
        connect(block, &CollapsibleBlock::expandedChanged, this, [block, this]() {
            bool isExpanded = block->isExpanded();
            int index = m_blocks.indexOf(block);
            if (index >= 0) {
                switch (index) {
                    case 0: setUIExpandedCorrecting(isExpanded); break;
                    case 1: setUIExpandedPreamplifier(isExpanded); break;
                    case 2: setUIExpandedEqualizer(isExpanded); break;
                    case 3: setUIExpandedReverb(isExpanded); break;
                    case 4: setUIExpandedSettings(isExpanded); break;
                }
            }
        });
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
    QString corrIRName = QFileInfo(QString::fromStdString(m_config.correctionIRFilePath)).completeBaseName();
    int corrComboIndex = -1;
    for (const auto &path : m_config.correctionRecent) {
        QString fname = QFileInfo(QString::fromStdString(path)).completeBaseName();
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
    sliderColumn->addStretch();
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
    connect(mixSlider, &QSlider::valueChanged, this, [mixSlider]() {
        updateSliderColor(mixSlider);
    });
    updateSliderColor(mixSlider);
    mixValueLabel->setText(QString::number(mixSlider->value()) + "%");
    sliderColumn->addWidget(mixSlider);
    sliderColumn->addStretch();

    QHBoxLayout *mixKnobLayout = new QHBoxLayout();
    mixKnobLayout->setContentsMargins(0, 0, 0, 0);
    mixKnobLayout->setSpacing(8);
    mixKnobLayout->addLayout(sliderColumn, 1);

    QVBoxLayout *knobLayout = new QVBoxLayout();
    knobLayout->setContentsMargins(0, 0, 0, 0);
    knobLayout->setSpacing(2);
    knobLayout->setAlignment(Qt::AlignCenter);
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

    connect(m_blocks[0], &CollapsibleBlock::toggled, this, [](bool checked) {
        setCorrectionToggle(checked);
    });
    connect(irCombo, &QComboBox::currentIndexChanged, this, [irCombo](int index) {
        QString path = irCombo->itemData(index).toString();
        if (!path.isEmpty())
            setCorrectionIRFile(path.toStdString());
    });
    connect(loadIrBtn, &QPushButton::clicked, this, [irCombo]() {
        QString file = QFileDialog::getOpenFileName(nullptr, "Select IR File", QDir::homePath(), "WAV Files (*.wav)");
        if (!file.isEmpty()) {
            setCorrectionIRFile(file.toStdString());
            irCombo->addItem(QFileInfo(file).completeBaseName(), file);
            irCombo->setCurrentIndex(irCombo->count() - 1);
        }
    });
    connect(mixSlider, &QSlider::valueChanged, this, [](int v) {
        setCorrectionDryWet(static_cast<double>(v) / 100.0);
    });
    connect(gainKnob, &KnobWidget::valueChanged, this, [](const QString &value) {
        QString num = value;
        num.remove(" dB");
        bool ok;
        float db = num.toFloat(&ok);
        if (ok) setCorrectionPostGain(db);
    });

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
    connect(gainSlider, &QSlider::valueChanged, this, [gainSlider]() {
        updateSliderColor(gainSlider);
    });
    updateSliderColor(gainSlider);
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

    connect(m_blocks[1], &CollapsibleBlock::toggled, this, [](bool checked) {
        setAmplifierToggle(checked);
    });
    connect(gainSlider, &QSlider::valueChanged, this, [](int v) {
        setAmplifierGain(static_cast<float>(v));
    });

    connect(autoSwitch, &QCheckBox::toggled, this, [this, gainSlider, gainValueLabel, autoSwitch](bool checked) {
        setAmplifierAuto(checked);
        gainSlider->setDisabled(checked);
        if (checked) {
            m_ampAutoTimer = new QTimer(this);
            connect(m_ampAutoTimer, &QTimer::timeout, this, [gainSlider, gainValueLabel, this]() {
                float v = getAmplifierAutoGainValue();
                int iv = qBound(-30, static_cast<int>(v + 0.5f), 30);
                if (gainSlider->value() != iv) {
                    gainSlider->blockSignals(true);
                    gainSlider->setValue(iv);
                    gainSlider->blockSignals(false);
                    gainValueLabel->setText(QString("%1%2 dB").arg(iv >= 0 ? "+" : "").arg(iv));
                    updateSliderColor(gainSlider);
                }
            });
            m_ampAutoTimer->start(200);
        } else {
            if (m_ampAutoTimer) {
                m_ampAutoTimer->stop();
                m_ampAutoTimer->deleteLater();
                m_ampAutoTimer = nullptr;
            }
        }
    });
    if (m_config.ampAuto) {
        autoSwitch->setChecked(true);
    }

    QWidget *equalizerContent = new QWidget();
    m_eqContent = equalizerContent;
    QVBoxLayout *eqLayout = new QVBoxLayout(equalizerContent);
    eqLayout->setContentsMargins(8, 4, 8, 4);
    eqLayout->setSpacing(4);

    QLabel *presetLabel = new QLabel(tr("Preset"));
    eqLayout->addWidget(presetLabel);
    QComboBox *presetCombo = new QComboBox();
    QString eqBasePath = QCoreApplication::applicationDirPath() + "/../Resources/eq/";
    QDir eqDir(eqBasePath);
    if (eqDir.exists()) {
        eqDir.setFilter(QDir::Files);
        eqDir.setNameFilters({"*.json"});
        eqDir.setSorting(QDir::Name);
        for (const auto &entry : eqDir.entryInfoList())
            presetCombo->addItem(entry.completeBaseName(), entry.filePath());
    }
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
        m_eqHz[i] = hzSpinBoxes[i];
        eqGrid->addWidget(hzSpinBoxes[i], 0, col);

        eqSliders[i] = new QSlider(Qt::Vertical);
        eqSliders[i]->setRange(-30, 30);
        eqSliders[i]->setValue(i < m_config.equalizerG.size() ? static_cast<int>(m_config.equalizerG[i]) : 0);
        eqSliders[i]->setFixedHeight(200);
        m_eqGain[i] = eqSliders[i];
        eqGrid->addWidget(eqSliders[i], 1, col, Qt::AlignHCenter);

        gainSpinboxes[i] = new QSpinBox();
        gainSpinboxes[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        gainSpinboxes[i]->setAlignment(Qt::AlignCenter);
        gainSpinboxes[i]->setRange(-30, 30);
        gainSpinboxes[i]->setValue(i < m_config.equalizerG.size() ? static_cast<int>(m_config.equalizerG[i]) : 0);
        m_eqGainSpin[i] = gainSpinboxes[i];
        eqGrid->addWidget(gainSpinboxes[i], 2, col);

        connect(eqSliders[i], &QSlider::valueChanged, gainSpinboxes[i], &QSpinBox::setValue);
        connect(gainSpinboxes[i], QOverload<int>::of(&QSpinBox::valueChanged), eqSliders[i], &QSlider::setValue);
        connect(eqSliders[i], &QSlider::valueChanged, this, [s = eqSliders[i]]() {
            updateSliderColor(s);
        });
        updateSliderColor(eqSliders[i]);

        qSpinboxes[i] = new QDoubleSpinBox();
        qSpinboxes[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        qSpinboxes[i]->setAlignment(Qt::AlignCenter);
        qSpinboxes[i]->setRange(0.1, 15.0);
        qSpinboxes[i]->setValue(i < m_config.equalizerQ.size() ? static_cast<double>(m_config.equalizerQ[i]) : 1.0);
        qSpinboxes[i]->setSingleStep(0.1);
        m_eqQ[i] = qSpinboxes[i];
        eqGrid->addWidget(qSpinboxes[i], 3, col);
    }

    QLabel *gainLabel2 = new QLabel("Gain");
    eqGrid->addWidget(gainLabel2, 2, 0);

    QLabel *qLabel = new QLabel("Q value");
    eqGrid->addWidget(qLabel, 3, 0);

    eqLayout->addLayout(eqGrid);

    connect(presetCombo, &QComboBox::currentIndexChanged, this, [presetCombo, this](int index) {
        QString path = presetCombo->itemData(index).toString();
        if (path.isEmpty())
            return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        QJsonArray fArr = obj["f"].toArray();
        QJsonArray qArr = obj["q"].toArray();
        QJsonArray gArr = obj["g"].toArray();
        for (int i = 0; i < BAND_COUNT; ++i) {
            if (i < fArr.size())
                m_eqHz[i]->setValue(fArr[i].toInt());
            if (i < gArr.size()) {
                int g = gArr[i].toInt();
                m_eqGain[i]->setValue(g);
                m_eqGainSpin[i]->setValue(g);
            }
            if (i < qArr.size())
                m_eqQ[i]->setValue(qArr[i].toDouble());
        }
    });

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

    connect(m_blocks[2], &CollapsibleBlock::toggled, this, [](bool checked) {
        setEqualizerToggle(checked);
    });

    auto syncBand = [this](int i) {
        float f = static_cast<float>(m_eqHz[i]->value());
        float g = static_cast<float>(m_eqGainSpin[i]->value());
        float q = static_cast<float>(m_eqQ[i]->value());
        setEqualizerBand(i, f, q, g);
    };
    for (int i = 0; i < BAND_COUNT; ++i) {
        connect(m_eqHz[i], &QSpinBox::valueChanged, this, [syncBand, i]()  { syncBand(i); });
        connect(m_eqGainSpin[i], &QSpinBox::valueChanged, this, [syncBand, i]() { syncBand(i); });
        connect(m_eqQ[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [syncBand, i]() { syncBand(i); });
    }

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

    QString irBasePath = QCoreApplication::applicationDirPath() + "/../Resources/ir/";
    QDir irDir(irBasePath);
    int correctReverbIdx = -1;
    if (irDir.exists()) {
        irDir.setFilter(QDir::Files);
        irDir.setNameFilters({"*.wav"});
        irDir.setSorting(QDir::Name);
        const auto entries = irDir.entryInfoList();
        for (const auto &entry : entries) {
            spaceCombo->addItem(entry.completeBaseName(), entry.filePath());
            if (entry.completeBaseName() == QFileInfo(QString::fromStdString(m_config.irFilePath)).completeBaseName())
                correctReverbIdx = spaceCombo->count() - 1;
        }
    }
    if (correctReverbIdx >= 0)
        spaceCombo->setCurrentIndex(correctReverbIdx);
    else if (!m_config.irFilePath.empty())
        spaceCombo->addItem(QFileInfo(QString::fromStdString(m_config.irFilePath)).completeBaseName(),
                             QString::fromStdString(m_config.irFilePath));

    connect(spaceCombo, &QComboBox::currentIndexChanged, this, [spaceCombo](int index) {
        QString path = spaceCombo->itemData(index).toString();
        if (!path.isEmpty())
            setReverbIRFile(path.toStdString());
    });

    spaceRowLayout->addWidget(spaceCombo, 1);
    QPushButton *customBtn = new QPushButton(tr("Custom"));

    connect(customBtn, &QPushButton::clicked, this, [spaceCombo]() {
        QString file = QFileDialog::getOpenFileName(nullptr, "Select IR File", QDir::homePath(), "WAV Files (*.wav)");
        if (!file.isEmpty()) {
            spaceCombo->addItem(QFileInfo(file).completeBaseName(), file);
            spaceCombo->setCurrentIndex(spaceCombo->count() - 1);
            setReverbIRFile(file.toStdString());
        }
    });

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
    connect(cvMixSlider, &QSlider::valueChanged, this, [cvMixSlider]() {
        updateSliderColor(cvMixSlider);
    });
    updateSliderColor(cvMixSlider);
    connect(cvMixSlider, &QSlider::valueChanged, this, [](int v) {
        setReverbDryWet(static_cast<double>(v) / 100.0);
    });
    cvMixValue->setText(QString::number(cvMixSlider->value()) + "%");
    cvLayout->addWidget(cvMixSlider);

    m_blocks[3]->setContentWidget(convolverContent);

    connect(m_blocks[3], &CollapsibleBlock::toggled, this, [](bool checked) {
        setReverbToggle(checked);
    });

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
    connect(outDevCombo, &QComboBox::currentTextChanged, this, [](const QString &text) {
        setOutputDevice(text.toStdString());
    });
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
    connect(bufSizeCombo, &QComboBox::currentIndexChanged, this, [bufSizeCombo](int index) {
        int bs = bufSizeCombo->itemData(index).toInt();
        if (bs > 0) setBufferSize(bs);
    });
    stLayout->addWidget(bufSizeCombo);

    m_blocks[4]->setContentWidget(settingsContent);

    setCentralWidget(centralWidget);
    m_blocks[0]->setExpanded(m_config.uiExpandedCorrecting);
    m_blocks[1]->setExpanded(m_config.uiExpandedPreamplifier);
    m_blocks[2]->setExpanded(m_config.uiExpandedEqualizer);
    m_blocks[3]->setExpanded(m_config.uiExpandedReverb);
    m_blocks[4]->setExpanded(m_config.uiExpandedSettings);
}

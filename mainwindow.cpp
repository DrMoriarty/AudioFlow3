#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "collapsibleblock.h"
#include "knobwidget.h"
#include "equalizergraph.h"
#include "correctiongraph.h"
#include "src/fileutils/config.h"
#include "src/fileutils/readIRFile.h"
#include "src/audioflow.h"

#include <regex>
#include <sstream>
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
#include "azimuthselector.h"
#include <QDir>
#include <QInputDialog>
#include <QLineEdit>
#include <QJsonDocument>
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

#include <QStatusBar>
#include <QPainter>

class VuBar : public QWidget {
public:
    explicit VuBar(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(8, 24);
    }
    void setLevel(float db) {
        float clamped = qBound(-30.0f, db, 0.0f);
        m_level = (clamped + 30.0f) / 30.0f;
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(30, 30, 30));
        int h = static_cast<int>(m_level * height());
        if (h > 0) {
            p.fillRect(0, height() - h, width(), h, QColor(60, 200, 60));
        }
    }
private:
    float m_level = 0.0f;
};

class LedIndicator : public QWidget {
public:
    enum State { None, Loaded, Error };
    explicit LedIndicator(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(12, 12);
    }
    void setState(State s, const QString &tooltip = {}) {
        if (m_state == s && m_tooltip == tooltip) return;
        m_state = s;
        m_tooltip = tooltip;
        setToolTip(tooltip);
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor c;
        switch (m_state) {
            case Loaded: c = QColor(60, 200, 60); break;
            case Error:  c = QColor(220, 50, 50); break;
            default:     c = QColor(80, 80, 80); break;
        }
        p.setBrush(c);
        p.setPen(QColor(40, 40, 40));
        p.drawEllipse(1, 1, width() - 2, height() - 2);
    }
private:
    State m_state = None;
    QString m_tooltip;
};

MainWindow::MainWindow(const Config &config, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_config(config)
{
    ui->setupUi(this);
    setupBlocks();

    m_latencyLabel = new QLabel();
    m_processLabel = new QLabel();
    m_vuL = new VuBar();
    m_vuR = new VuBar();
    statusBar()->addWidget(new QLabel(" L"));
    statusBar()->addWidget(m_vuL);
    statusBar()->addWidget(m_vuR);
    statusBar()->addWidget(new QLabel("R "));
    statusBar()->addPermanentWidget(m_processLabel);
    statusBar()->addPermanentWidget(m_latencyLabel);
    statusBar()->setStyleSheet("QStatusBar{border-top:1px solid rgb(60,60,60);} QLabel{font-size:10px;}");

    auto *vuTimer = new QTimer(this);
    connect(vuTimer, &QTimer::timeout, this, [this]() {
        static_cast<VuBar*>(m_vuL)->setLevel(getPeakLevelL());
        static_cast<VuBar*>(m_vuR)->setLevel(getPeakLevelR());
    });
    vuTimer->start(50);

    auto *statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, [this]() {
        m_latencyLabel->setText(QString("Latency: %1 ms").arg(getLatencyMs(), 0, 'f', 1));
        m_processLabel->setText(QString("Process: %1 ms").arg(getProcessTimeMs(), 0, 'f', 2));
        {
            auto s = getCorrectionIRStatus();
            if (!s.hasFile)
                m_correctionLed->setState(LedIndicator::None);
            else if (s.loaded)
                m_correctionLed->setState(LedIndicator::Loaded, tr("Loaded file %1 s").arg(s.duration, 0, 'f', 2));
            else
                m_correctionLed->setState(LedIndicator::Error, tr("Error"));
        }
        {
            auto s = getReverbIRStatus();
            if (!s.hasFile)
                m_reverbLed->setState(LedIndicator::None);
            else if (s.loaded)
                m_reverbLed->setState(LedIndicator::Loaded, tr("Loaded file %1 s").arg(s.duration, 0, 'f', 2));
            else
                m_reverbLed->setState(LedIndicator::Error, tr("Error"));
        }
    });
    statusTimer->start(1000);

    m_latencyLabel->setText(QString("Latency: %1 ms").arg(getLatencyMs(), 0, 'f', 1));
    m_processLabel->setText(QString("Process: %1 ms").arg(getProcessTimeMs(), 0, 'f', 2));
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

static QString matchConfigValue(const std::string& raw, const std::string& selCfg) {
    QString q = QString::fromStdString(raw).trimmed();
    if (q.isEmpty() || q == "N/A" || selCfg.empty()) return q == "N/A" ? QString() : q;

    QString result;
    bool matched = false;
    QString bareValue;
    int start = 0;
    int depth = 0;

    for (int i = 0; i < q.size() && !matched; ++i) {
        if (q[i] == '(') ++depth;
        else if (q[i] == ')') {
            if (--depth == 0) {
                int end = i + 1;
                QString entry = q.mid(start, end - start).trimmed();
                int firstParen = entry.indexOf('(');
                bool isFirstEntry = (start == 0);
                if (firstParen >= 0) {
                    QString name = entry.left(firstParen);
                    if (isFirstEntry) name = name.trimmed();
                    else {
                        name = name.trimmed();
                        if (!name.isEmpty() && name[0] == QChar(',')) name = name.mid(1).trimmed();
                    }
                    int lastOpen = entry.lastIndexOf('(');
                    int lastClose = entry.lastIndexOf(')');
                    QString inside = entry.mid(lastOpen + 1, lastClose - lastOpen - 1).trimmed();
                    QStringList cfgs = inside.split(',', Qt::SkipEmptyParts);
                    bool found = false;
                    for (QString& c : cfgs) {
                        if (c.trimmed().toStdString() == selCfg) { found = true; break; }
                    }
                    if (found) {
                        result = name;
                        matched = true;
                    }
                }
                if (!matched && firstParen < 0) bareValue = entry;
                start = end;
            }
        } else if (q[i] == ',' && depth == 0) {
            int end = i;
            QString entry = q.mid(start, end - start).trimmed();
            int firstParen = entry.indexOf('(');
            bool isFirstEntry = (start == 0);
            if (firstParen < 0) {
                if (!entry.isEmpty()) bareValue = isFirstEntry ? entry : (bareValue.isEmpty() ? entry : bareValue);
            }
            start = i + 1;
        }
    }

    if (!matched) {
        if (start < q.size()) {
            QString entry = q.mid(start).trimmed();
            int firstParen = entry.indexOf('(');
            bool isFirstEntry = (start == 0);
            if (firstParen < 0) {
                if (!entry.isEmpty()) bareValue = isFirstEntry ? entry : (bareValue.isEmpty() ? entry : bareValue);
            }
        }
    }

    if (matched) {
        if (!result.isEmpty() && result[0] == QChar(',')) result = result.mid(1).trimmed();
        return result;
    }
    if (!bareValue.isEmpty() && bareValue[0] == QChar(',')) bareValue = bareValue.mid(1).trimmed();
    if (!bareValue.isEmpty()) return bareValue;
    return QString();
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
        tr("Binaural Simulation"),
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
                    case 4: setUIExpandedBinaural(isExpanded); break;
                    case 5: setUIExpandedSettings(isExpanded); break;
                }
            }
        });
        mainLayout->addWidget(block);
        m_blocks.append(block);
        if (i < 5)
            block->addToggleSwitch();
    }
    m_blocks[0]->setToggleChecked(m_config.correctionToggle);
    m_blocks[1]->setToggleChecked(m_config.ampToggle);
    m_blocks[2]->setToggleChecked(m_config.equalizerToggle);
    m_blocks[3]->setToggleChecked(m_config.reverbToggle);
    m_blocks[4]->setToggleChecked(m_config.binauralToggle);
    m_blockCollapsedHeight = m_blocks[0]->headerHeight();
    m_expandedHeights.resize(m_blocks.size(), -1);

    QWidget *correctingContent = new QWidget();
    QVBoxLayout *ccLayout = new QVBoxLayout(correctingContent);
    ccLayout->setContentsMargins(8, 4, 8, 4);
    ccLayout->setSpacing(4);

    QLabel *descLabel = new QLabel(tr("Correcting the frequency response of your audio device"));
    descLabel->setWordWrap(true);
    ccLayout->addWidget(descLabel);

    m_correctionGraph = new AudioFlow3::CorrectionGraph();
    m_correctionGraph->setFixedHeight(100);
    ccLayout->addWidget(m_correctionGraph);

    auto updateCorrectionGraph = [this](const std::string& path, double dryWet) {
        if (!m_correctionGraph || path.empty()) {
            if (m_correctionGraph) m_correctionGraph->clear();
            return;
        }
        IRData irData = readIRFile(path);
        if (!irData.audioDataL.empty()) {
            m_correctionGraph->setIRData(irData.audioDataL, irData.audioDataR, static_cast<int>(irData.sampleRate));
            m_correctionGraph->setDryWet(dryWet);
        }
    };

    updateCorrectionGraph(m_config.correctionIRFilePath, m_config.correctionDryWet);

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
    m_correctionLed = new LedIndicator();
    irLayout->addWidget(m_correctionLed);
    QPushButton *loadIrBtn = new QPushButton(tr("Load IR"));
    irLayout->addWidget(loadIrBtn);

    QVBoxLayout *mixKnobColumn = new QVBoxLayout();
    mixKnobColumn->setContentsMargins(0, 0, 0, 0);
    mixKnobColumn->setSpacing(2);
    mixKnobColumn->setAlignment(Qt::AlignCenter);
    QLabel *mixLabel = new QLabel(tr("Dry/Wet"));
    mixLabel->setAlignment(Qt::AlignCenter);
    mixKnobColumn->addWidget(mixLabel);
    KnobWidget *mixKnob = new KnobWidget(0.0, 100.0, 1.0, m_config.correctionDryWet * 100.0, "%");
    mixKnobColumn->addWidget(mixKnob);
    irLayout->addLayout(mixKnobColumn);

    QVBoxLayout *knobColumn = new QVBoxLayout();
    knobColumn->setContentsMargins(0, 0, 0, 0);
    knobColumn->setSpacing(2);
    knobColumn->setAlignment(Qt::AlignCenter);
    QLabel *gainLabel = new QLabel(tr("Wet Gain"));
    gainLabel->setAlignment(Qt::AlignCenter);
    knobColumn->addWidget(gainLabel);
    KnobWidget *gainKnob = new KnobWidget(0.0, 9.0, 1.0, m_config.correctionPostGain, "dB");
    knobColumn->addWidget(gainKnob);
    irLayout->addLayout(knobColumn);
    ccLayout->addLayout(irLayout);

    m_blocks[0]->setContentWidget(correctingContent);

    connect(m_blocks[0], &CollapsibleBlock::toggled, this, [](bool checked) {
        setCorrectionToggle(checked);
    });
    connect(irCombo, &QComboBox::currentIndexChanged, this, [irCombo, mixKnob, updateCorrectionGraph](int index) {
        QString path = irCombo->itemData(index).toString();
        if (!path.isEmpty()) {
            setCorrectionIRFile(path.toStdString());
            double num = mixKnob->currentNumericValue();
            updateCorrectionGraph(path.toStdString(), num / 100.0);
        }
    });
    connect(loadIrBtn, &QPushButton::clicked, this, [irCombo, mixKnob, updateCorrectionGraph]() {
        QString file = QFileDialog::getOpenFileName(nullptr, "Select IR File", QDir::homePath(), "WAV Files (*.wav)");
        if (!file.isEmpty()) {
            setCorrectionIRFile(file.toStdString());
            irCombo->addItem(QFileInfo(file).completeBaseName(), file);
            irCombo->setCurrentIndex(irCombo->count() - 1);
            double num = mixKnob->currentNumericValue();
            updateCorrectionGraph(file.toStdString(), num / 100.0);
        }
    });
    connect(mixKnob, &KnobWidget::valueChanged, this, [this](double value) {
        setCorrectionDryWet(value / 100.0);
        if (m_correctionGraph) m_correctionGraph->setDryWet(value / 100.0);
    });
    connect(gainKnob, &KnobWidget::valueChanged, this, [](double value) {
        setCorrectionPostGain(value);
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
    gainSlider->setRange(-12, 12);
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
    QLabel *minLabel = new QLabel(tr("-12 dB"));
    sliderLabelLayout->addWidget(minLabel);
    sliderLabelLayout->addStretch();
    QLabel *zeroLabel = new QLabel(tr("0 dB"));
    sliderLabelLayout->addWidget(zeroLabel);
    sliderLabelLayout->addStretch();
    QLabel *maxLabel = new QLabel(tr("+12 dB"));
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
    m_eqPresetCombo = presetCombo;

    QString eqBasePath = QCoreApplication::applicationDirPath() + "/../Resources/eq/";
    QDir eqDir(eqBasePath);
    if (eqDir.exists()) {
        eqDir.setFilter(QDir::Files);
        eqDir.setNameFilters({"*.json"});
        eqDir.setSorting(QDir::Name);
        for (const auto &entry : eqDir.entryInfoList())
            presetCombo->addItem(entry.completeBaseName(), entry.filePath());
    }

    QString customDir = QString::fromStdString(m_config.customPresetsDirPath);
    QDir cDir(customDir);
    if (cDir.exists()) {
        cDir.setFilter(QDir::Files);
        cDir.setNameFilters({"*.json"});
        cDir.setSorting(QDir::Name);
        for (const auto &entry : cDir.entryInfoList())
            presetCombo->addItem(entry.completeBaseName(), entry.filePath());
    }

    QPushButton *savePresetBtn = new QPushButton(tr("Save"));
    savePresetBtn->setFixedWidth(60);

    QHBoxLayout *presetRow = new QHBoxLayout();
    presetRow->setContentsMargins(0, 0, 0, 0);
    presetRow->setSpacing(4);
    presetRow->addWidget(presetCombo);
    presetRow->addWidget(savePresetBtn);
    eqLayout->addLayout(presetRow);

    if (!m_config.equalizerPreset.empty()) {
        QString activePreset = QString::fromStdString(m_config.equalizerPreset);
        for (int i = 0; i < presetCombo->count(); ++i) {
            if (presetCombo->itemText(i) == activePreset) {
                presetCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    AudioFlow3::EqualizerGraph *eqGraph = new AudioFlow3::EqualizerGraph();
    eqGraph->setFixedHeight(100);
    m_eqGraph = eqGraph;
    eqLayout->addWidget(eqGraph);

    QLabel *parametersLabel = new QLabel(tr("Parameters"));
    eqLayout->addWidget(parametersLabel);

    // Update graph with initial data
    const int defaultHz[] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    QVector<double> freqs(BAND_COUNT);
    QVector<double> gains(BAND_COUNT);
    QVector<double> qs(BAND_COUNT);
    for (int i = 0; i < BAND_COUNT; ++i) {
        freqs[i] = i < m_config.equalizerF.size() ? static_cast<double>(m_config.equalizerF[i]) : static_cast<double>(defaultHz[i]);
        gains[i] = i < m_config.equalizerG.size() ? static_cast<double>(m_config.equalizerG[i]) : 0.0;
        qs[i] = i < m_config.equalizerQ.size() ? static_cast<double>(m_config.equalizerQ[i]) : 1.0;
    }
    eqGraph->setFrequencyData(freqs);
    eqGraph->setGainData(gains);
    eqGraph->setQData(qs);
    eqGraph->setControlData(freqs, gains);

    QGridLayout *eqGrid = new QGridLayout();
    eqGrid->setContentsMargins(0, 0, 0, 2);
    eqGrid->setSpacing(2);

    QLabel *hzLabel = new QLabel("Hz");
    eqGrid->addWidget(hzLabel, 0, 0);

    QWidget *eqScaleLabels = new QWidget();
    QVBoxLayout *eqScaleLayout = new QVBoxLayout(eqScaleLabels);
    eqScaleLayout->setContentsMargins(0, 0, 0, 0);
    eqScaleLayout->setSpacing(0);

    QLabel *eqPlusLabel = new QLabel("+12 dB");
    eqPlusLabel->setAlignment(Qt::AlignCenter);
    QLabel *eqZeroLabel = new QLabel("0 dB");
    eqZeroLabel->setAlignment(Qt::AlignCenter);
    QLabel *eqMinusLabel = new QLabel("-12 dB");
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
        eqSliders[i]->setRange(-12, 12);
        eqSliders[i]->setValue(i < m_config.equalizerG.size() ? static_cast<int>(m_config.equalizerG[i]) : 0);
        eqSliders[i]->setPageStep(1);
        eqSliders[i]->setFixedHeight(100);
        m_eqGain[i] = eqSliders[i];
        eqGrid->addWidget(eqSliders[i], 1, col, Qt::AlignHCenter);

        gainSpinboxes[i] = new QSpinBox();
        gainSpinboxes[i]->setButtonSymbols(QAbstractSpinBox::NoButtons);
        gainSpinboxes[i]->setAlignment(Qt::AlignCenter);
        gainSpinboxes[i]->setRange(-12, 12);
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
        m_eqPresetLoading = true;
        QString path = presetCombo->itemData(index).toString();
        if (path.isEmpty()) {
            m_eqPresetLoading = false;
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            m_eqPresetLoading = false;
            return;
        }
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
        setEqualizerPreset(presetCombo->currentText().toStdString());
        m_eqPresetLoading = false;
    });

    connect(savePresetBtn, &QPushButton::clicked, this, [presetCombo, this]() {
        bool ok;
        QString name = QInputDialog::getText(this, tr("Save Preset"),
            tr("Preset name:"), QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        name = name.trimmed();

        QJsonObject obj;
        QJsonArray fArr, qArr, gArr;
        for (int i = 0; i < BAND_COUNT; ++i) {
            fArr.append(m_eqHz[i]->value());
            qArr.append(m_eqQ[i]->value());
            gArr.append(m_eqGain[i]->value());
        }
        obj["f"] = fArr;
        obj["q"] = qArr;
        obj["g"] = gArr;

        QString dirPath = QString::fromStdString(getConfig().customPresetsDirPath);
        QDir().mkpath(dirPath);
        QString filePath = dirPath + "/" + name + ".json";
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly))
            return;
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));

        bool found = false;
        for (int i = 0; i < presetCombo->count(); ++i) {
            if (presetCombo->itemData(i).toString() == filePath) {
                presetCombo->setItemText(i, name);
                presetCombo->setCurrentIndex(i);
                found = true;
                break;
            }
        }
        if (!found) {
            presetCombo->addItem(name, filePath);
            presetCombo->setCurrentIndex(presetCombo->count() - 1);
        }
        setEqualizerPreset(name.toStdString());
    });

    int eqContentH = eqLayout->contentsMargins().top() + eqLayout->contentsMargins().bottom();
    eqContentH += presetLabel->sizeHint().height() + presetCombo->sizeHint().height();
    eqContentH += parametersLabel->sizeHint().height();
    eqContentH += eqLayout->spacing() * 3;
    eqContentH += 100; // EqualizerGraph
    int sliderH = 100; // fixed slider height
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

    auto updateGraph = [this]() {
        QVector<double> freqs(BAND_COUNT), gains(BAND_COUNT), qs(BAND_COUNT);
        for (int i = 0; i < BAND_COUNT; ++i) {
            freqs[i] = static_cast<double>(m_eqHz[i]->value());
            gains[i] = static_cast<double>(m_eqGainSpin[i]->value());
            qs[i] = static_cast<double>(m_eqQ[i]->value());
        }
        if (m_eqGraph) {
            m_eqGraph->setFrequencyData(freqs);
            m_eqGraph->setGainData(gains);
            m_eqGraph->setQData(qs);
            m_eqGraph->setControlData(freqs, gains);
        }
    };

    for (int i = 0; i < BAND_COUNT; ++i) {
        connect(m_eqHz[i], &QSpinBox::valueChanged, this, [syncBand, presetCombo, i, this, updateGraph]() {
            syncBand(i);
            updateGraph();
            if (!m_eqPresetLoading) {
                presetCombo->blockSignals(true);
                presetCombo->setCurrentIndex(-1);
                presetCombo->blockSignals(false);
                if (!getConfig().equalizerPreset.empty()) {
                    setEqualizerPreset("");
                }
            }
        });
        connect(m_eqGainSpin[i], &QSpinBox::valueChanged, this, [syncBand, presetCombo, i, this, updateGraph]() {
            syncBand(i);
            updateGraph();
            if (!m_eqPresetLoading) {
                presetCombo->blockSignals(true);
                presetCombo->setCurrentIndex(-1);
                presetCombo->blockSignals(false);
                if (!getConfig().equalizerPreset.empty()) {
                    setEqualizerPreset("");
                }
            }
        });
        connect(m_eqQ[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [syncBand, presetCombo, i, this, updateGraph]() {
            syncBand(i);
            updateGraph();
            if (!m_eqPresetLoading) {
                presetCombo->blockSignals(true);
                presetCombo->setCurrentIndex(-1);
                presetCombo->blockSignals(false);
                if (!getConfig().equalizerPreset.empty()) {
                    setEqualizerPreset("");
                }
            }
        });
    }

    connect(presetCombo, &QComboBox::currentIndexChanged, this, [presetCombo, this, updateGraph](int) {
        updateGraph();
    });

    QWidget *convolverContent = new QWidget();
    QHBoxLayout *cvLayout = new QHBoxLayout(convolverContent);
    cvLayout->setContentsMargins(8, 4, 8, 4);
    cvLayout->setSpacing(4);

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
    m_reverbLed = new LedIndicator();
    spaceRowLayout->addWidget(m_reverbLed);
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

    QVBoxLayout *leftColumn = new QVBoxLayout();
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(4);
    QLabel *spaceLabel = new QLabel(tr("Space"));
    leftColumn->addWidget(spaceLabel);
    QLabel *spacer = new QLabel();
    spacer->setContentsMargins(0, 0, 0, 0);
    spacer->setFixedSize(1, 1);
    leftColumn->addWidget(spacer);
    leftColumn->addStretch(1);
    spaceRowLayout->setAlignment(Qt::AlignCenter);
    leftColumn->addLayout(spaceRowLayout);
    QLabel *bottomSpacer = new QLabel();
    bottomSpacer->setContentsMargins(0, 0, 0, 0);
    bottomSpacer->setFixedSize(1, 18);
    leftColumn->addWidget(bottomSpacer);
    cvLayout->addLayout(leftColumn);

    QVBoxLayout *rightColumn = new QVBoxLayout();
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->setSpacing(2);
    rightColumn->setAlignment(Qt::AlignCenter);
    QLabel *cvMixLabel = new QLabel(tr("Dry/Wet"));
    cvMixLabel->setAlignment(Qt::AlignCenter);
    rightColumn->addWidget(cvMixLabel);
    KnobWidget *cvMixKnob = new KnobWidget(0.0, 100.0, 1.0, m_config.reverbDryWet * 100.0, "%");
    rightColumn->addWidget(cvMixKnob);
    cvLayout->addLayout(rightColumn);

    connect(cvMixKnob, &KnobWidget::valueChanged, this, [this](double value) {
        setReverbDryWet(value / 100.0);
    });

    m_blocks[3]->setContentWidget(convolverContent);

    connect(m_blocks[3], &CollapsibleBlock::toggled, this, [this](bool checked) {
        if (!m_toggleGuard && checked) {
            m_toggleGuard = true;
            m_blocks[4]->setToggleChecked(false);
            setBinauralToggle(false);
            m_toggleGuard = false;
        }
        setReverbToggle(checked);
    });

    QWidget *binauralContent = new QWidget();
    QHBoxLayout *bnLayout = new QHBoxLayout(binauralContent);
    bnLayout->setContentsMargins(8, 4, 8, 4);
    bnLayout->setSpacing(4);

    // --- Wrap all current controls in a vertical box ---
    QWidget *bnControlsWidget = new QWidget();
    QVBoxLayout *bnControlsLayout = new QVBoxLayout(bnControlsWidget);
    bnControlsLayout->setContentsMargins(0, 0, 0, 0);
    bnControlsLayout->setSpacing(4);

    auto roomInfos = getRoomInfos();
    std::sort(roomInfos.begin(), roomInfos.end(), [](const RoomInfoData& a, const RoomInfoData& b) {
        if (a.type != b.type) return a.type < b.type;
        return a.name < b.name;
    });

    QHBoxLayout *bnRoomLayout = new QHBoxLayout();
    bnRoomLayout->setContentsMargins(0, 0, 0, 0);
    bnRoomLayout->setSpacing(8);
    QLabel *bnRoomLabel = new QLabel(tr("Room"));
    bnRoomLayout->addWidget(bnRoomLabel);
    QComboBox *bnRoomCombo = new QComboBox();

    for (const auto& room : roomInfos) {
        QString displayName = QString::fromStdString(room.name)
            + ", " + QString::fromStdString(room.type);
        bnRoomCombo->addItem(displayName, QString::fromStdString(room.id));
    }
    for (int i = 0; i < bnRoomCombo->count(); ++i) {
        if (bnRoomCombo->itemData(i).toString().toStdString() == m_config.binauralRoom) {
            bnRoomCombo->setCurrentIndex(i);
            break;
        }
    }
    bnRoomLayout->addWidget(bnRoomCombo, 1);
    bnControlsLayout->addLayout(bnRoomLayout);

    QWidget *bnInfoWidget = new QWidget();
    bnInfoWidget->setStyleSheet("background: rgba(255,255,255,0.04); border-radius: 4px;");
    QGridLayout *bnGrid = new QGridLayout(bnInfoWidget);
    bnGrid->setContentsMargins(8, 6, 8, 6);
    bnGrid->setSpacing(2);

    const QString lblSt = "font-size: 10px; color: #888;";
    const QString valSt = "font-size: 11px; color: #ddd;";

    auto makeValLbl = [&](const QString& st) {
        QLabel *l = new QLabel();
        l->setStyleSheet(st);
        return l;
    };

    QLabel *lblLocation  = new QLabel("Room Location");   lblLocation->setStyleSheet(lblSt);
    QLabel *lblType      = new QLabel("Room Type");       lblType->setStyleSheet(lblSt);
    QLabel *lblDim       = new QLabel("Room Dimensions"); lblDim->setStyleSheet(lblSt);
    QLabel *lblListener  = new QLabel("Listener");        lblListener->setStyleSheet(lblSt);

    QLabel *valLocation = makeValLbl(valSt);
    QLabel *valType     = makeValLbl(valSt);
    QLabel *valDim      = makeValLbl(valSt);
    QLabel *valListener = makeValLbl(valSt);

    QLabel *lblRt60   = new QLabel("RT60");             lblRt60->setStyleSheet(lblSt);
    QLabel *lblAz     = new QLabel("Azimuth Range");    lblAz->setStyleSheet(lblSt);
    QLabel *lblDist   = new QLabel("Source Distance");  lblDist->setStyleSheet(lblSt);

    QLabel *valRt60 = makeValLbl(valSt);
    QLabel *valAz   = makeValLbl(valSt);
    QLabel *valDist = makeValLbl(valSt);

    int row = 0;
    bnGrid->addWidget(lblLocation,  row, 0); bnGrid->addWidget(valLocation, row, 1, 1, 3); row++;
    bnGrid->addWidget(lblType,      row, 0); bnGrid->addWidget(valType,     row, 1, 1, 3); row++;
    bnGrid->addWidget(lblListener,  row, 0); bnGrid->addWidget(valListener, row, 1, 1, 3); row++;

    bnGrid->addItem(new QSpacerItem(1,8), row, 0, 1, 4); row++;

    bnGrid->addWidget(lblDist, row, 0); bnGrid->addWidget(valDist, row, 1);
    bnGrid->addWidget(lblRt60, row, 2); bnGrid->addWidget(valRt60, row, 3); row++;

    bnGrid->addItem(new QSpacerItem(1,4), row, 0, 1, 4); row++;

    bnGrid->addWidget(lblAz, row, 0); bnGrid->addWidget(valAz, row, 1);
    bnGrid->addWidget(lblDim, row, 2); bnGrid->addWidget(valDim, row, 3); row++;

    bnControlsLayout->addWidget(bnInfoWidget);

    // --- Config row ---
    QHBoxLayout *bnCfgLayout = new QHBoxLayout();
    bnCfgLayout->setContentsMargins(0, 0, 0, 0);
    bnCfgLayout->setSpacing(8);
    QLabel *bnCfgLabel = new QLabel("Configuration");
    bnCfgLayout->addWidget(bnCfgLabel);
    QComboBox *bnCfgCombo = new QComboBox();
    bnCfgLayout->addWidget(bnCfgCombo);
    bnCfgLayout->addStretch();
    QCheckBox *bnTrueStereo = new QCheckBox("True Stereo");
    bnCfgLayout->addWidget(bnTrueStereo);
    bnCfgLayout->addStretch();
    bnControlsLayout->addLayout(bnCfgLayout);

    auto updateRoomInfo = [bnRoomCombo, bnCfgCombo, roomInfos,
        valLocation, valType, valDim, valListener, valRt60, valAz, valDist]() {
        std::string roomId = bnRoomCombo->currentData().toString().toStdString();
        std::string selCfg = bnCfgCombo->currentText().toStdString();
        auto it = std::find_if(roomInfos.begin(), roomInfos.end(),
            [&](const RoomInfoData& r){ return r.id == roomId; });
        if (it == roomInfos.end()) return;
        const auto& r = *it;
        auto sv = [](const std::string& v, const std::string& cfg, const std::string& suffix = {}) -> QString {
            QString m = matchConfigValue(v, cfg);
            if (m.isEmpty()) return QString();
            return suffix.empty() ? m : m + " " + QString::fromStdString(suffix);
        };
        auto showOrHide = [](QLabel* val, QLabel* lbl, const QString& s) {
            if (s.isEmpty()) { if (lbl) lbl->hide(); val->hide(); return; }
            if (lbl) lbl->show(); val->show();
            val->setText(s);
        };
        showOrHide(valLocation, nullptr, sv(r.location, selCfg));
        showOrHide(valType,     nullptr, sv(r.type, selCfg));
        showOrHide(valDim,      nullptr, sv(r.dimensions, selCfg, "m"));
        showOrHide(valListener, nullptr, sv(r.listener, selCfg));
        showOrHide(valRt60,     nullptr, sv(r.rt60, selCfg, "s"));
        showOrHide(valAz,       nullptr, sv(r.azimuthRange, selCfg));
        showOrHide(valDist,     nullptr, sv(r.sourceDistance, selCfg, "m"));
    };

    auto updateConfigOptions = [bnCfgCombo, bnCfgLabel, roomInfos, bnRoomCombo]() {
        std::string roomId = bnRoomCombo->currentData().toString().toStdString();
        auto it = std::find_if(roomInfos.begin(), roomInfos.end(),
            [&](const RoomInfoData& r){ return r.id == roomId; });
        if (it == roomInfos.end()) return;
        bnCfgCombo->blockSignals(true);
        bnCfgCombo->clear();
        if (!it->measurementConfig.empty() && it->measurementConfig != "N/A") {
            std::stringstream ss(it->measurementConfig);
            std::string item;
            while (std::getline(ss, item, ',')) {
                auto pos = item.find_first_not_of(" \t");
                if (pos != std::string::npos) item = item.substr(pos);
                auto posEnd = item.find_last_not_of(" \t\"\n\r");
                if (posEnd != std::string::npos) item = item.substr(0, posEnd + 1);
                if (!item.empty()) bnCfgCombo->addItem(QString::fromStdString(item));
            }
        } else {
            bnCfgCombo->addItem("C1");
        }
        bnCfgCombo->blockSignals(false);
        bool multipleCfg = bnCfgCombo->count() > 1;
        bnCfgCombo->setEnabled(multipleCfg);
        bnCfgLabel->setEnabled(multipleCfg);
    };

    updateConfigOptions();
    updateRoomInfo();

    // --- AzimuthSelector on the right ---
    AzimuthSelector *bnAzimuthSelector = new AzimuthSelector(
        static_cast<double>(std::round(m_config.binauralAngle / 5.0)) * 5.0,
        180.0);
    connect(bnAzimuthSelector, &AzimuthSelector::angleChanged, this, [bnAzimuthSelector](double angle) {
        setBinauralAngle(static_cast<int>(std::round(angle)));
    });

    auto updateAngleRange = [bnRoomCombo, roomInfos, bnAzimuthSelector]() {
        int maxAz = 150;
        std::string roomId = bnRoomCombo->currentData().toString().toStdString();
        auto it = std::find_if(roomInfos.begin(), roomInfos.end(),
            [&](const RoomInfoData& r){ return r.id == roomId; });
        if (it != roomInfos.end()) {
            std::string az = it->azimuthRange;
            std::smatch m;
            if (std::regex_search(az, m, std::regex("±(\\d+)")))
                maxAz = std::stoi(m[1].str());
            else if (std::regex_search(az, m, std::regex("[+-]?(\\d+)\\s*°")))
                maxAz = std::stoi(m[1].str());
            bnAzimuthSelector->setMaxAngle(static_cast<double>(maxAz));
        }
    };

    bnLayout->addWidget(bnControlsWidget);
    bnLayout->addWidget(bnAzimuthSelector, 1);

    updateAngleRange();

    connect(bnCfgCombo, &QComboBox::currentIndexChanged, this, [bnCfgCombo, updateRoomInfo, updateAngleRange]() {
        updateRoomInfo();
        updateAngleRange();
        QString cfg = bnCfgCombo->currentText();
        if (!cfg.isEmpty()) setBinauralConfig(cfg.toStdString());
    });

    m_blocks[4]->setContentWidget(binauralContent);

    connect(bnRoomCombo, &QComboBox::currentIndexChanged, this,
        [bnRoomCombo, bnAzimuthSelector,
         bnTrueStereo,
         updateRoomInfo, updateConfigOptions, updateAngleRange, bnCfgCombo]() {
        QString room = bnRoomCombo->currentData().toString();
        updateConfigOptions();
        if (!room.isEmpty()) {
            QString cfg = bnCfgCombo->currentText();
            setBinauralRoom(room.toStdString(), cfg.toStdString());
        }
        updateAngleRange();
        updateRoomInfo();
        if (bnTrueStereo->isChecked()) {
            bnAzimuthSelector->setEnabled(false);
        }
    });

    // Restore saved room → config → angle from config
    {
        for (int i = 0; i < bnRoomCombo->count(); ++i) {
            if (bnRoomCombo->itemData(i).toString().toStdString() == m_config.binauralRoom) {
                bnRoomCombo->setCurrentIndex(i);
                break;
            }
        }
        for (int i = 0; i < bnCfgCombo->count(); ++i) {
            if (bnCfgCombo->itemText(i).toStdString() == m_config.binauralConfig) {
                bnCfgCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    bnTrueStereo->setChecked(m_config.binauralTrueStereo);
    if (m_config.binauralTrueStereo) {
        bnAzimuthSelector->setEnabled(false);
    }
    connect(bnTrueStereo, &QCheckBox::toggled, this,
        [bnAzimuthSelector, updateAngleRange]
        (bool checked) {
            setBinauralTrueStereo(checked);
            if (checked) {
                bnAzimuthSelector->setEnabled(false);
            } else {
                updateAngleRange();
                bnAzimuthSelector->setEnabled(true);
            }
        }
    );

    connect(m_blocks[4], &CollapsibleBlock::toggled, this, [this](bool checked) {
        if (!m_toggleGuard && checked) {
            m_toggleGuard = true;
            m_blocks[3]->setToggleChecked(false);
            setReverbToggle(false);
            m_toggleGuard = false;
        }
        setBinauralToggle(checked);
    });

    QWidget *settingsContent = new QWidget();
    QGridLayout *stLayout = new QGridLayout(settingsContent);
    stLayout->setContentsMargins(8, 4, 8, 4);
    stLayout->setSpacing(4);

    // === Output device ===
    QLabel *outDevLabel = new QLabel(tr("Output device"));
    outDevLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    outDevLabel->setMinimumWidth(outDevLabel->fontMetrics().horizontalAdvance(tr("Output device")) + 10);
    stLayout->addWidget(outDevLabel, 0, 0, 1, 1);

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
    stLayout->addWidget(outDevCombo, 1, 0, 1, 2);

    // === Buffer size ===
    QLabel *bufSizeLabel = new QLabel(tr("Buffer size"));
    bufSizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bufSizeLabel->setMinimumWidth(bufSizeLabel->fontMetrics().horizontalAdvance(tr("Buffer size")) + 10);
    stLayout->addWidget(bufSizeLabel, 0, 2, 1, 1);

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
    stLayout->addWidget(bufSizeCombo, 1, 2, 1, 2);

    m_blocks[5]->setContentWidget(settingsContent);

    setCentralWidget(centralWidget);
    m_blocks[0]->setExpanded(m_config.uiExpandedCorrecting);
    m_blocks[1]->setExpanded(m_config.uiExpandedPreamplifier);
    m_blocks[2]->setExpanded(m_config.uiExpandedEqualizer);
    m_blocks[3]->setExpanded(m_config.uiExpandedReverb);
    m_blocks[4]->setExpanded(m_config.uiExpandedBinaural);
    m_blocks[5]->setExpanded(m_config.uiExpandedSettings);
}

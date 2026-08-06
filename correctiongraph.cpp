#include "correctiongraph.h"
#include <Accelerate/Accelerate.h>
#include <cmath>
#include <algorithm>

namespace AudioFlow3 {

static constexpr double CR_LOG10_20 = 1.30103;
static constexpr double CR_LOG10_20000 = 4.30103;
static constexpr int CR_DISPLAY_POINTS = 100;

CorrectionGraph::CorrectionGraph(QWidget *parent) : QWidget(parent) {
}

CorrectionGraph::~CorrectionGraph() {
}

static size_t nextPow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

void CorrectionGraph::setIRData(const std::vector<float>& dataL, const std::vector<float>& dataR, int sampleRate) {
    m_hasData = false;
    m_cachedControlPoints.clear();

    if (dataL.empty() && dataR.empty()) {
        update();
        return;
    }

    size_t irLen = std::max(dataL.size(), dataR.size());
    if (irLen == 0) {
        update();
        return;
    }

    size_t maxFFT = 131072;
    if (irLen > maxFFT)
        irLen = maxFFT;

    size_t N = nextPow2(irLen);
    size_t halfN = N / 2;

    std::vector<float> bufL(N, 0.0f);
    std::vector<float> bufR(N, 0.0f);
    std::copy(dataL.begin(), dataL.begin() + std::min(dataL.size(), irLen), bufL.begin());
    std::copy(dataR.begin(), dataR.begin() + std::min(dataR.size(), irLen), bufR.begin());

    std::vector<float> reL(halfN), imL(halfN);
    std::vector<float> reR(halfN), imR(halfN);

    FFTSetup fftSetup = vDSP_create_fftsetup(static_cast<vDSP_Length>(std::log2(N)), FFT_RADIX2);

    auto fft = [&](const std::vector<float>& in, std::vector<float>& outRe, std::vector<float>& outIm) {
        std::vector<float> tmp(N, 0.0f);
        std::copy(in.begin(), in.end(), tmp.begin());
        DSPSplitComplex sc = { outRe.data(), outIm.data() };
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(tmp.data()), 2, &sc, 1, halfN);
        vDSP_fft_zrip(fftSetup, &sc, 1, static_cast<vDSP_Length>(std::log2(N)), FFT_FORWARD);
        float scale = 1.0f / static_cast<float>(N);
        vDSP_vsmul(outRe.data(), 1, &scale, outRe.data(), 1, halfN);
        vDSP_vsmul(outIm.data(), 1, &scale, outIm.data(), 1, halfN);
    };

    fft(bufL, reL, imL);
    fft(bufR, reR, imR);

    vDSP_destroy_fftsetup(fftSetup);

    double fs = static_cast<double>(sampleRate);
    double binRes = fs / static_cast<double>(N);

    double peakMagL = 0.0, peakMagR = 0.0;
    for (size_t j = 0; j < halfN; ++j) {
        peakMagL = std::max(peakMagL, static_cast<double>(std::hypot(reL[j], imL[j])));
        peakMagR = std::max(peakMagR, static_cast<double>(std::hypot(reR[j], imR[j])));
    }

    m_freqHz.resize(CR_DISPLAY_POINTS);
    m_magL.resize(CR_DISPLAY_POINTS);
    m_magR.resize(CR_DISPLAY_POINTS);

    double peakL = (peakMagL > 0.0) ? peakMagL : 1.0;
    double peakR = (peakMagR > 0.0) ? peakMagR : 1.0;

    for (int i = 0; i < CR_DISPLAY_POINTS; ++i) {
        double t = static_cast<double>(i) / (CR_DISPLAY_POINTS - 1);
        double f = std::pow(10.0, CR_LOG10_20 + t * (CR_LOG10_20000 - CR_LOG10_20));
        m_freqHz[i] = f;

        double binF = f / binRes;
        int k = static_cast<int>(binF);
        double frac = binF - k;

        double magL, magR;
        if (k >= static_cast<int>(halfN) - 1) {
            magL = std::hypot(reL[halfN - 1], imL[halfN - 1]) / peakL;
            magR = std::hypot(reR[halfN - 1], imR[halfN - 1]) / peakR;
        } else {
            double aL = std::hypot(reL[k], imL[k]);
            double bL = std::hypot(reL[k + 1], imL[k + 1]);
            magL = (aL + frac * (bL - aL)) / peakL;
            double aR = std::hypot(reR[k], imR[k]);
            double bR = std::hypot(reR[k + 1], imR[k + 1]);
            magR = (aR + frac * (bR - aR)) / peakR;
        }

        m_magL[i] = std::max(0.0, magL);
        m_magR[i] = std::max(0.0, magR);
    }

    m_hasData = true;
    m_dirty = true;
    update();
}

void CorrectionGraph::setDryWet(double value) {
    if (value == m_dryWet) return;
    m_dryWet = value;
    m_dirty = true;
    update();
}

void CorrectionGraph::clear() {
    m_hasData = false;
    m_dirty = true;
    m_cachedControlPoints.clear();
    m_freqHz.clear();
    m_magL.clear();
    m_magR.clear();
    update();
}

void CorrectionGraph::showEvent(QShowEvent *) {
    update();
}

void CorrectionGraph::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::black);

    if (m_hasData) {
        if (m_dirty) {
            generateCurves();
            m_dirty = false;
        }

        QVector<QPointF> screenCorrection;
        QVector<QPointF> screenOriginal;
        screenCorrection.reserve(m_cachedControlPoints.size());
        screenOriginal.reserve(m_cachedOriginalPoints.size());
        for (const auto& p : m_cachedControlPoints)
            screenCorrection.append(convertToScreenCoordinates(p.x(), p.y()));
        for (const auto& p : m_cachedOriginalPoints)
            screenOriginal.append(convertToScreenCoordinates(p.x(), p.y()));
        QVector<QPointF> correctionPoints = catmullRomSpline(screenCorrection, 4);
        QVector<QPointF> originalPoints = catmullRomSpline(screenOriginal, 4);

        drawAxes(painter);
        drawFrequencyMarks(painter);
        drawDBMarks(painter);
        drawGraph(painter, correctionPoints, Qt::blue, QColor(0, 0, 255, 100));
        drawGraph(painter, originalPoints, Qt::white, Qt::NoBrush);
    } else {
        drawAxes(painter);
        drawFrequencyMarks(painter);
        drawDBMarks(painter);
    }
}

void CorrectionGraph::generateCurves() {
    m_cachedControlPoints.clear();
    m_cachedOriginalPoints.clear();
    m_cachedControlPoints.reserve(CR_DISPLAY_POINTS);
    m_cachedOriginalPoints.reserve(CR_DISPLAY_POINTS);

    double t = m_dryWet;
    constexpr double MIND = -24.0;
    constexpr double MID_LIN = 0.251188643150958;
    constexpr double ORIG_SCALE = 0.0630957344480193;

    for (int i = 0; i < CR_DISPLAY_POINTS; ++i) {
        double avgMag = 0.5 * (m_magL[i] + m_magR[i]);
        double corrMix = (1.0 - t) * MID_LIN + t * avgMag;
        double dBCorr = std::clamp(20.0 * std::log10(corrMix), MIND, 0.0);
        double origNatLin = ORIG_SCALE / std::max(avgMag, 1e-12);
        double origMix = (1.0 - t) * origNatLin + t * MID_LIN;
        double dBOrig = std::clamp(20.0 * std::log10(origMix), MIND, 0.0);
        m_cachedControlPoints.append({m_freqHz[i], dBCorr});
        m_cachedOriginalPoints.append({m_freqHz[i], dBOrig});
    }
}

QVector<QPointF> CorrectionGraph::catmullRomSpline(const QVector<QPointF>& pts, int subdivisions) const {
    if (pts.size() < 2) return pts;

    QVector<QPointF> result;
    result.reserve((pts.size() - 1) * subdivisions + 1);

    for (int i = 0; i < pts.size() - 1; ++i) {
        QPointF p0 = pts[std::max(0, i - 1)];
        QPointF p1 = pts[i];
        QPointF p2 = pts[i + 1];
        QPointF p3 = pts[std::min(static_cast<int>(pts.size()) - 1, i + 2)];

        for (int j = 0; j < subdivisions; ++j) {
            double t = static_cast<double>(j) / subdivisions;
            double t2 = t * t;
            double t3 = t2 * t;

            double x = 0.5 * ((2.0 * p1.x()) +
                              (-p0.x() + p2.x()) * t +
                              (2.0 * p0.x() - 5.0 * p1.x() + 4.0 * p2.x() - p3.x()) * t2 +
                              (-p0.x() + 3.0 * p1.x() - 3.0 * p2.x() + p3.x()) * t3);
            double y = 0.5 * ((2.0 * p1.y()) +
                              (-p0.y() + p2.y()) * t +
                              (2.0 * p0.y() - 5.0 * p1.y() + 4.0 * p2.y() - p3.y()) * t2 +
                              (-p0.y() + 3.0 * p1.y() - 3.0 * p2.y() + p3.y()) * t3);

            result.append({x, y});
        }
    }
    result.append(pts.last());

    return result;
}

QPointF CorrectionGraph::convertToScreenCoordinates(double f, double db) const {
    double logF = std::log10(f);
    double x = (logF - CR_LOG10_20) / (CR_LOG10_20000 - CR_LOG10_20);
    x = width() * x;

    double y = (db + 24.0) / 24.0;
    y = height() * (1.0 - y);

    return QPointF(x, y);
}

void CorrectionGraph::drawAxes(QPainter& painter) const {
    painter.setPen(Qt::white);
    painter.drawLine(0, 0, 0, height());
    painter.drawLine(0, height(), width(), height());
}

void CorrectionGraph::drawFrequencyMarks(QPainter& painter) const {
    painter.setPen(Qt::white);
    QVector<double> marks = {100, 1000, 10000};
    for (double f : marks) {
        QPointF point = convertToScreenCoordinates(f, -24.0);
        painter.drawLine(point, QPointF(point.x(), 0));
    }

    painter.setPen(Qt::gray);
    QVector<double> marks2 = {20, 30, 40, 50, 200, 300, 400, 500, 2000, 3000, 4000, 5000};
    for (double f : marks2) {
        QPointF point = convertToScreenCoordinates(f, -24.0);
        painter.drawLine(point, QPointF(point.x(), 0));
    }
}

void CorrectionGraph::drawDBMarks(QPainter& painter) const {
    painter.setPen(Qt::gray);
    QVector<double> marks = {-18, -12, -6, 0};
    for (double db : marks) {
        QPointF point = convertToScreenCoordinates(20.0, db);
        painter.drawLine(QPointF(0, point.y()), QPointF(width(), point.y()));
    }
}

void CorrectionGraph::drawGraph(QPainter& painter, const QVector<QPointF>& points, const QColor& lineColor, const QColor& fillColor) const {
    QPen graphPen(lineColor);
    graphPen.setWidthF(3.0);
    painter.setPen(graphPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(points);

    if (fillColor != Qt::NoBrush) {
        QVector<QPointF> fillPoints = points;
        fillPoints.append(QPointF(width(), height()));
        fillPoints.append(QPointF(0, height()));
        painter.setBrush(fillColor);
        painter.drawPolygon(fillPoints);
    }
}

} // namespace AudioFlow3

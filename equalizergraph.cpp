#include "equalizergraph.h"

namespace AudioFlow3 {

static constexpr double LOG10_20 = 1.30103;
static constexpr double LOG10_20000 = 4.30103;

EqualizerGraph::EqualizerGraph(QWidget *parent) : QWidget(parent) {
    m_sampleRate = 44100;
}

EqualizerGraph::~EqualizerGraph() {
}

void EqualizerGraph::setFrequencyData(const QVector<double>& frequencies) {
    m_frequencies = frequencies;
    m_dirty = true;
    update();
}

void EqualizerGraph::setGainData(const QVector<double>& gains) {
    m_gains = gains;
    m_dirty = true;
    update();
}

void EqualizerGraph::setQData(const QVector<double>& qValues) {
    m_qValues = qValues;
    m_dirty = true;
    update();
}

void EqualizerGraph::setSampleRate(int sampleRate) {
    m_sampleRate = sampleRate;
    m_dirty = true;
    update();
}

void EqualizerGraph::showEvent(QShowEvent *event) {
    update();
}

void EqualizerGraph::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    painter.fillRect(rect(), Qt::black);

    if (m_dirty) {
        m_cachedControlPoints = generateControlPoints();
        m_dirty = false;
    }

    QVector<QPointF> screenControl;
    for (const auto& p : m_cachedControlPoints)
        screenControl.append(convertToScreenCoordinates(p.x(), p.y()));
    QVector<QPointF> graphPoints = catmullRomSpline(screenControl, 4);

    // Draw axes and marks
    drawAxes(painter);
    drawFrequencyMarks(painter);
    drawGainMarks(painter);

    // Draw the equalizer graph
    drawGraph(painter, graphPoints);
}

QVector<QPointF> EqualizerGraph::generateControlPoints() const {
    QVector<QPointF> points;
    constexpr int N = 100;
    points.reserve(N);

    if (m_frequencies.isEmpty()) return points;

    double logMin = LOG10_20;
    double logMax = LOG10_20000;

    for (int i = 0; i < N; ++i) {
        double t = static_cast<double>(i) / (N - 1);
        double f = std::pow(10.0, logMin + t * (logMax - logMin));

        double totalGain = 0.0;
        for (int j = 0; j < m_frequencies.size(); ++j) {
            double bandwidth = m_frequencies[j] / m_qValues[j];
            double distance = std::abs(f - m_frequencies[j]);
            double influence = std::exp(-0.5 * (distance / bandwidth) * (distance / bandwidth));
            totalGain += m_gains[j] * influence;
        }
        totalGain = std::max(-12.0, std::min(12.0, totalGain));

        points.append({f, totalGain});
    }

    return points;
}

QVector<QPointF> EqualizerGraph::catmullRomSpline(const QVector<QPointF>& pts, int subdivisions) const {
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

QPointF EqualizerGraph::convertToScreenCoordinates(double f, double gain) const {
    // Frequency axis (log scale) — precomputed constants
    double logF = std::log10(f);
    double x = (logF - LOG10_20) / (LOG10_20000 - LOG10_20);
    x = width() * x;

    // Gain axis (linear scale, inverted)
    double y = (gain + 12.0) / 24.0;
    y = height() * (1.0 - y);

    return QPointF(x, y);
}

void EqualizerGraph::drawAxes(QPainter& painter) const {
    painter.setPen(Qt::white);
    painter.drawLine(0, 0, 0, height());
    painter.drawLine(0, height(), width(), height());
}

void EqualizerGraph::drawFrequencyMarks(QPainter& painter) const {
    painter.setPen(Qt::white);
    QVector<double> marks = {100, 1000, 10000};
    for (double f : marks) {
        QPointF point = convertToScreenCoordinates(f, -12.0);
        painter.drawLine(point, QPointF(point.x(), 0));
    }

    painter.setPen(Qt::gray);
    QVector<double> marks2 = {20, 30, 40, 50, 200, 300, 400, 500, 2000, 3000, 4000, 5000};
    for (double f : marks2) {
        QPointF point = convertToScreenCoordinates(f, -12.0);
        painter.drawLine(point, QPointF(point.x(), 0));
    }
}

void EqualizerGraph::drawGainMarks(QPainter& painter) const {
    painter.setPen(Qt::gray);
    QVector<double> marks = {-6, 0, 6};
    for (double g : marks) {
        QPointF point = convertToScreenCoordinates(20.0, g);
        painter.drawLine(QPointF(0, point.y()), QPointF(width(), point.y()));
    }
}

void EqualizerGraph::drawGraph(QPainter& painter, const QVector<QPointF>& points) const {
    QPen graphPen(Qt::blue);
    graphPen.setWidthF(3.0);
    painter.setPen(graphPen);
    painter.setBrush(Qt::blue);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(points);

    // Fill under the curve
    QVector<QPointF> fillPoints = points;
    fillPoints.append(QPointF(width(), height()));
    fillPoints.append(QPointF(0, height()));
    painter.setBrush(QColor(0, 0, 255, 100));
    painter.drawPolygon(fillPoints);
}

} // namespace AudioFlow3

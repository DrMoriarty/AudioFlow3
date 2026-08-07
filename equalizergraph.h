#pragma once

#include <QWidget>
#include <QPainter>
#include <QVector>

namespace AudioFlow3 {

class EqualizerGraph : public QWidget {
    Q_OBJECT
public:
    explicit EqualizerGraph(QWidget *parent = nullptr);
    ~EqualizerGraph();

    void setFrequencyData(const QVector<double>& frequencies);
    void setGainData(const QVector<double>& gains);
    void setQData(const QVector<double>& qValues);
    void setSampleRate(int sampleRate);
    void setControlData(const QVector<double>& frequencies, const QVector<double>& gains);

    // Required for Qt's painting system
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QVector<double> m_frequencies;
    QVector<double> m_gains;
    QVector<double> m_qValues;
    int m_sampleRate;
    bool m_dirty = true;
    QVector<QPointF> m_cachedControlPoints;
    QVector<QPointF> m_cachedControlPointsRaw;

    // Graph generation with per-point summation of all band contributions
    QVector<QPointF> generateControlPoints() const;
    QVector<QPointF> catmullRomSpline(const QVector<QPointF>& pts, int subdivisions) const;
    QPointF convertToScreenCoordinates(double f, double gain) const;
    void drawAxes(QPainter& painter) const;
    void drawFrequencyMarks(QPainter& painter) const;
    void drawGainMarks(QPainter& painter) const;
    void drawControlPoints(QPainter& painter) const;
    void drawGraph(QPainter& painter, const QVector<QPointF>& points) const;
};

} // namespace AudioFlow3
#pragma once

#include <QWidget>
#include <QPainter>
#include <QVector>
#include <vector>

namespace AudioFlow3 {

class CorrectionGraph : public QWidget {
    Q_OBJECT
public:
    explicit CorrectionGraph(QWidget *parent = nullptr);
    ~CorrectionGraph();

    void setIRData(const std::vector<float>& dataL, const std::vector<float>& dataR, int sampleRate);
    void setDryWet(double value);
    void clear();

    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    bool m_dirty = false;
    bool m_hasData = false;
    double m_dryWet = 1.0;

    QVector<double> m_freqHz;
    QVector<double> m_magL;
    QVector<double> m_magR;

    QVector<QPointF> m_cachedControlPoints;
    QVector<QPointF> m_cachedOriginalPoints;

    void generateCurves();
    QVector<QPointF> catmullRomSpline(const QVector<QPointF>& pts, int subdivisions) const;
    QPointF convertToScreenCoordinates(double f, double db) const;
    void drawAxes(QPainter& painter) const;
    void drawFrequencyMarks(QPainter& painter) const;
    void drawDBMarks(QPainter& painter) const;
    void drawGraph(QPainter& painter, const QVector<QPointF>& points, const QColor& lineColor, const QColor& fillColor) const;
};

} // namespace AudioFlow3

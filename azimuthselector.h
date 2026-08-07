#ifndef AZIMUTHSELECTOR_H
#define AZIMUTHSELECTOR_H

#include <QWidget>
#include <QPointF>
#include <QLineF>
#include <QPainterPath>

class QPainter;
class QMouseEvent;
class QWheelEvent;
class QPropertyAnimation;

class AzimuthSelector : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double angle READ angle WRITE setAngle)
    Q_PROPERTY(double maxAngle READ maxAngle WRITE setMaxAngle)

public:
    explicit AzimuthSelector(double initialAngle = 90.0, double maxAngle = 180.0, QWidget *parent = nullptr);

    double angle() const;
    void setAngle(double angle);
    double maxAngle() const;
    void setMaxAngle(double maxAngle);

signals:
    void angleChanged(double angle);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateSources();
    double degreesToRadians(double degrees) const;

    static constexpr int WIDGET_WIDTH = 150;
    static constexpr int WIDGET_HEIGHT = 150;

    double m_angle;
    double m_maxAngle;
    bool m_mousePressed = false;
    bool m_dragging = false;
    QPointF m_pressCenter;
    double m_dragStartAngle;
    QPropertyAnimation *m_anim = nullptr;
    QLineF m_arcPath;
    QPainterPath m_triPath;
    QPointF m_srcL;
    QPointF m_srcR;
    int m_triSide;
};

#endif // AZIMUTHSELECTOR_H

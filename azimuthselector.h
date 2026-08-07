#ifndef AZIMUTHSELECTOR_H
#define AZIMUTHSELECTOR_H

#include <QWidget>
#include <QLabel>
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
    void updateLabel();
    void animateToAngle(double targetAngle, int durationMs);
    double degreesToRadians(double degrees) const;

    static constexpr int WIDGET_WIDTH = 150;
    static constexpr int WIDGET_HEIGHT = 150;
    static constexpr int CLICK_MAX_PX = 16;
    static constexpr int ANIMATION_DURATION_MS = 250;
    static constexpr int SNAP_DURATION_MS = 125;
    static constexpr int DRAG_STEP_PX_BASE = 300;

    double m_angle;
    double m_maxAngle;
    bool m_mousePressed = false;
    bool m_dragging = false;
    QPointF m_pressStart;
    double m_dragStartAngle;
    double m_endAngle;
    QPropertyAnimation *m_anim = nullptr;
    QLineF m_arcPath;
    QPointF m_srcL;
    QPointF m_srcR;
    int m_triSide;
    QLabel *m_label = nullptr;
};

#endif // AZIMUTHSELECTOR_H

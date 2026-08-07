#ifndef KNOBWIDGET_H
#define KNOBWIDGET_H

#include <QWidget>

class QLabel;
class QPropertyAnimation;
class QWheelEvent;

class KnobWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double currentAngle READ currentAngle WRITE setCurrentAngle)

public:
    explicit KnobWidget(double min, double max, double step, const QString &suffix = QString(), QWidget *parent = nullptr);
explicit KnobWidget(double min, double max, double step, double initialValue, const QString &suffix = QString(), QWidget *parent = nullptr);

    double currentNumericValue() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    double currentAngle() const;
    void setCurrentAngle(double angle);

signals:
    void valueChanged(double value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateLabel();
    double indexToAngle(int index) const;
    int angleToNearestIndex(double angle) const;
    void animateToAngle(double targetAngle, int durationMs);

    double m_min;
    double m_max;
    double m_step;
    QString m_suffix;
    int m_index;
    QLabel *m_label;

    static constexpr int CLICK_MAX_PX = 16;
    static constexpr int ANIMATION_DURATION_MS = 250;
    static constexpr int SNAP_DURATION_MS = 125;
    static constexpr int DRAG_STEP_PX_BASE = 300;

    bool m_mousePressed = false;
    bool m_dragging = false;
    QPoint m_pressStart;
    int m_dragStartIndex = 0;
    int m_totalSteps = 0;
    int m_dragStepPx = 0;
    int m_wheelDelta = 0;

    double m_currentAngle = 225.0;
    int m_endIndex = 0;
    QPropertyAnimation *m_anim = nullptr;
};

#endif // KNOBWIDGET_H

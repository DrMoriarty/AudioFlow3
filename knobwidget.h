#ifndef KNOBWIDGET_H
#define KNOBWIDGET_H

#include <QWidget>
#include <QStringList>

class QLabel;

class KnobWidget : public QWidget
{
    Q_OBJECT

public:
    explicit KnobWidget(const QStringList &values, QWidget *parent = nullptr);

    QString currentValue() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

signals:
    void valueChanged(const QString &value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void updateLabel();
    double indexToAngle(int index) const;
    int angleToNearestIndex(double angle) const;
    void startAnimation();
    void stopAnimation();
    void onAnimationTick();

    QStringList m_values;
    int m_index;
    QLabel *m_label;

    static constexpr int CLICK_MAX_PX = 16;
    static constexpr int DRAG_STEP_PX = 60;
    static constexpr int ANIMATION_DURATION_MS = 250;
    static constexpr int SNAP_DURATION_MS = 125;

    // Mouse interaction state
    bool m_mousePressed = false;
    bool m_dragging = false;
    QPoint m_pressStart;
    int m_dragStartIndex = 0;

    // Animation state
    bool m_animating = false;
    int m_endIndex = 0;
    double m_animStartAngle = 0.0;
    double m_animEndAngle = 0.0;
    double m_currentAngle = 225.0;
    QTimer *m_animTimer = nullptr;
    qint64 m_animStartTime = 0;
    int m_animDuration = ANIMATION_DURATION_MS;
};

#endif // KNOBWIDGET_H

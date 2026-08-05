#ifndef KNOBWIDGET_H
#define KNOBWIDGET_H

#include <QWidget>
#include <QStringList>

class QLabel;
class QPropertyAnimation;

class KnobWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double currentAngle READ currentAngle WRITE setCurrentAngle)

public:
    explicit KnobWidget(const QStringList &values, QWidget *parent = nullptr);

    QString currentValue() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    double currentAngle() const;
    void setCurrentAngle(double angle);

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
    void animateToAngle(double targetAngle, int durationMs);

    QStringList m_values;
    int m_index;
    QLabel *m_label;

    static constexpr int CLICK_MAX_PX = 16;
    static constexpr int DRAG_STEP_PX = 60;
    static constexpr int ANIMATION_DURATION_MS = 250;
    static constexpr int SNAP_DURATION_MS = 125;

    bool m_mousePressed = false;
    bool m_dragging = false;
    QPoint m_pressStart;
    int m_dragStartIndex = 0;

    double m_currentAngle = 225.0;
    int m_endIndex = 0;
    QPropertyAnimation *m_anim = nullptr;
};

#endif // KNOBWIDGET_H

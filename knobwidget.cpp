#include "knobwidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineF>
#include <QtMath>
#include <QPropertyAnimation>
#include <cmath>

KnobWidget::KnobWidget(const QStringList &values, QWidget *parent)
    : QWidget(parent)
    , m_values(values)
    , m_index(0)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);

    setFixedSize(60, 60);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_label);

    m_anim = new QPropertyAnimation(this, "currentAngle", this);
    m_anim->setEasingCurve(QEasingCurve::OutQuad);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        m_index = m_endIndex;
        updateLabel();
    });

    m_currentAngle = indexToAngle(m_index);
    updateLabel();
}

QString KnobWidget::currentValue() const
{
    return m_values.value(m_index);
}

int KnobWidget::currentIndex() const
{
    return m_index;
}

void KnobWidget::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_values.size()) {
        m_anim->stop();
        m_index = index;
        m_currentAngle = indexToAngle(index);
        updateLabel();
        update();
    }
}

double KnobWidget::currentAngle() const
{
    return m_currentAngle;
}

void KnobWidget::setCurrentAngle(double angle)
{
    m_currentAngle = angle;
    update();
}

double KnobWidget::indexToAngle(int index) const
{
    if (m_values.size() <= 1)
        return 225.0;
    return 225.0 + static_cast<double>(index) / static_cast<double>(m_values.size() - 1) * (-270.0);
}

int KnobWidget::angleToNearestIndex(double angle) const
{
    if (m_values.size() <= 1)
        return 0;
    double exact = (angle - 225.0) / (-270.0 / (m_values.size() - 1));
    return qBound(0, static_cast<int>(std::round(exact)), m_values.size() - 1);
}

void KnobWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int knobSize = 36;
    int knobX = (width() - knobSize) / 2;
    int knobY = 4;
    int radius = knobSize / 2;
    int cx = knobX + radius;
    int cy = knobY + radius;

    p.setPen(QPen(QColor("#808080"), 2));
    p.setBrush(QColor("#404040"));
    p.drawEllipse(cx - radius, cy - radius, knobSize, knobSize);

    if (m_values.size() > 1) {
        double rad = qDegreesToRadians(m_currentAngle);
        int lineLen = radius - 4;
        int ex = cx + static_cast<int>(lineLen * qCos(rad));
        int ey = cy - static_cast<int>(lineLen * qSin(rad));
        p.setPen(QPen(QColor("#ffffff"), 2));
        p.drawLine(cx, cy, ex, ey);
    }
}

void KnobWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_anim->state() == QAbstractAnimation::Running) {
            m_index = m_endIndex;
            m_currentAngle = indexToAngle(m_index);
            m_anim->stop();
        }

        m_pressStart = event->position().toPoint();
        m_dragStartIndex = m_index;
        m_mousePressed = true;
        m_dragging = false;

    } else if (event->button() == Qt::RightButton) {
        if (m_anim->state() == QAbstractAnimation::Running) {
            m_index = m_endIndex;
            m_currentAngle = indexToAngle(m_index);
            m_anim->stop();
        }

        if (m_index > 0) {
            m_endIndex = m_index - 1;
            m_index = m_endIndex;
            updateLabel();
            animateToAngle(indexToAngle(m_endIndex), ANIMATION_DURATION_MS);
        }
    }
}

void KnobWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mousePressed)
        return;

    QPoint pos = event->position().toPoint();
    QPoint delta = pos - m_pressStart;

    if (!m_dragging && delta.manhattanLength() > CLICK_MAX_PX) {
        m_dragging = true;
    }

    if (!m_dragging)
        return;

    m_anim->stop();

    int yDelta = pos.y() - m_pressStart.y();
    int xDelta = pos.x() - m_pressStart.x();
    double anglePerStep = -270.0 / (m_values.size() - 1);
    double angleDelta = ((-static_cast<double>(yDelta) + static_cast<double>(xDelta)) / DRAG_STEP_PX) * anglePerStep;
    double targetAngle = indexToAngle(m_dragStartIndex) + angleDelta;

    double minAngle = indexToAngle(m_values.size() - 1);
    double maxAngle = indexToAngle(0);
    m_currentAngle = qBound(minAngle, targetAngle, maxAngle);

    int nearest = angleToNearestIndex(m_currentAngle);
    if (nearest != m_index) {
        m_index = nearest;
        updateLabel();
    }
    update();
}

void KnobWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_mousePressed)
        return;

    if (!m_dragging) {
        if (m_index < m_values.size() - 1) {
            m_endIndex = m_index + 1;
            m_index = m_endIndex;
            updateLabel();
            animateToAngle(indexToAngle(m_endIndex), ANIMATION_DURATION_MS);
        }
    } else {
        int nearest = angleToNearestIndex(m_currentAngle);
        m_index = nearest;
        m_endIndex = nearest;
        animateToAngle(indexToAngle(nearest), SNAP_DURATION_MS);
    }

    m_mousePressed = false;
    m_dragging = false;
}

void KnobWidget::updateLabel()
{
    m_label->setText(m_values.value(m_index));
}

void KnobWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit valueChanged(currentValue());
}

void KnobWidget::animateToAngle(double targetAngle, int durationMs)
{
    m_anim->stop();
    m_anim->setStartValue(m_currentAngle);
    m_anim->setEndValue(targetAngle);
    m_anim->setDuration(durationMs);
    m_anim->start();
}

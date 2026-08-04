#include "knobwidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineF>
#include <QtMath>
#include <QTimer>
#include <QDateTime>
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
        m_index = index;
        m_currentAngle = indexToAngle(m_index);
        updateLabel();
        update();
    }
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
        // Cancel any running animation, snap to its target first
        if (m_animating) {
            m_index = m_endIndex;
            m_currentAngle = indexToAngle(m_index);
            stopAnimation();
        }

        m_pressStart = event->position().toPoint();
        m_dragStartIndex = m_index;
        m_mousePressed = true;
        m_dragging = false;

    } else if (event->button() == Qt::RightButton) {
        // Cancel any running animation, snap to its target first
        if (m_animating) {
            m_index = m_endIndex;
            m_currentAngle = indexToAngle(m_index);
            stopAnimation();
        }

        // Right click — previous value, no wrap-around
        if (m_index > 0) {
            int saved = m_index;
            m_index = m_index - 1;
            m_endIndex = m_index;
            m_animStartAngle = indexToAngle(saved);
            m_animEndAngle = indexToAngle(m_endIndex);
            m_animating = true;
            m_animStartTime = QDateTime::currentMSecsSinceEpoch();
            m_animDuration = ANIMATION_DURATION_MS;

            if (!m_animTimer) {
                m_animTimer = new QTimer(this);
                connect(m_animTimer, &QTimer::timeout, this, &KnobWidget::onAnimationTick);
            }
            m_animTimer->start(16);
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

    stopAnimation();

    int yDelta = pos.y() - m_pressStart.y();
    double anglePerStep = -270.0 / (m_values.size() - 1);
    double angleDelta = (-static_cast<double>(yDelta) / DRAG_STEP_PX) * anglePerStep;
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
        // Click — next value, no wrap-around
        if (m_index < m_values.size() - 1) {
            int saved = m_index;
            m_index = m_index + 1;
            m_endIndex = m_index;
            m_animStartAngle = indexToAngle(saved);
            m_animEndAngle = indexToAngle(m_endIndex);
            m_animating = true;
            m_animStartTime = QDateTime::currentMSecsSinceEpoch();
            m_animDuration = ANIMATION_DURATION_MS;

            if (!m_animTimer) {
                m_animTimer = new QTimer(this);
                connect(m_animTimer, &QTimer::timeout, this, &KnobWidget::onAnimationTick);
            }
            m_animTimer->start(16);
        }
    } else {
        // Drag released — animate to nearest discrete value
        int nearest = angleToNearestIndex(m_currentAngle);
        m_index = nearest;
        m_endIndex = nearest;
        m_animStartAngle = m_currentAngle;
        m_animEndAngle = indexToAngle(nearest);
        m_animating = true;
        m_animStartTime = QDateTime::currentMSecsSinceEpoch();
        m_animDuration = SNAP_DURATION_MS;

        if (!m_animTimer) {
            m_animTimer = new QTimer(this);
            connect(m_animTimer, &QTimer::timeout, this, &KnobWidget::onAnimationTick);
        }
        m_animTimer->start(16);
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

void KnobWidget::startAnimation()
{
    m_animStartAngle = m_currentAngle;
    m_animEndAngle = indexToAngle(m_endIndex);
    m_animating = true;
    m_animStartTime = QDateTime::currentMSecsSinceEpoch();
    m_animDuration = ANIMATION_DURATION_MS;

    if (!m_animTimer) {
        m_animTimer = new QTimer(this);
        connect(m_animTimer, &QTimer::timeout, this, &KnobWidget::onAnimationTick);
    }
    m_animTimer->start(16);
}

void KnobWidget::stopAnimation()
{
    if (m_animTimer) {
        m_animTimer->stop();
    }
    m_animating = false;
}

void KnobWidget::onAnimationTick()
{
    if (!m_animating) return;

    qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_animStartTime;
    double ratio = qBound(0.0, static_cast<double>(elapsedMs) / m_animDuration, 1.0);

    m_currentAngle = m_animStartAngle + (m_animEndAngle - m_animStartAngle) * ratio;

    if (ratio >= 1.0) {
        m_index = m_endIndex;
        m_currentAngle = m_animEndAngle;
        stopAnimation();
    }

    updateLabel();
    update();
}

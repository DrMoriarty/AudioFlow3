#include "azimuthselector.h"

#include <QPainter>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLineF>
#include <QPointF>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QtMath>
#include <cmath>

AzimuthSelector::AzimuthSelector(double initialAngle, double maxAngle, QWidget *parent)
    : QWidget(parent)
    , m_angle(initialAngle)
    , m_maxAngle(maxAngle)
{
    setFixedSize(WIDGET_WIDTH, WIDGET_HEIGHT);
    setCursor(Qt::PointingHandCursor);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setGeometry(QRect(0, WIDGET_HEIGHT / 2 + 30, WIDGET_WIDTH, 20));
    m_label->setText(QString::number(static_cast<int>(initialAngle)) + "°");

    m_anim = new QPropertyAnimation(this, "angle", this);
    m_anim->setDuration(150);
    m_anim->setEasingCurve(QEasingCurve::InOutCubic);
}

double AzimuthSelector::angle() const
{
    return m_angle;
}

void AzimuthSelector::setAngle(double angle)
{
    angle = qBound(0.0, angle, m_maxAngle);
    if (m_angle != angle) {
        m_angle = angle;
        updateSources();
        updateLabel();
        update();
        emit angleChanged(m_angle);
    }
}

double AzimuthSelector::maxAngle() const
{
    return m_maxAngle;
}

void AzimuthSelector::setMaxAngle(double maxAngle)
{
    m_maxAngle = qBound(0.0, maxAngle, 180.0);
    setAngle(m_angle);
}

void AzimuthSelector::updateSources()
{
    int cx = WIDGET_WIDTH / 2;
    int cy = WIDGET_HEIGHT / 2;
    double rad = degreesToRadians(m_angle);
    double sourceDist = 60.0;

    QPointF srcL(cx + static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));
    QPointF srcR(cx - static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));

    QLineF arcLine(srcL, srcR);

    m_arcPath = arcLine;
    m_srcL = srcL;
    m_srcR = srcR;
}

double AzimuthSelector::degreesToRadians(double degrees) const
{
    return degrees * M_PI / 180.0;
}

void AzimuthSelector::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int cx = WIDGET_WIDTH / 2;
    int cy = WIDGET_HEIGHT / 2;
    int radius = 60;

    bool disabled = !isEnabled();

    p.setPen(QPen(QColor(128, 128, 128), 1));
    p.drawEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

    double rad = degreesToRadians(m_angle);
    double sourceDist = radius;

    QPointF srcL(cx + static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));
    QPointF srcR(cx - static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));

    p.setPen(QPen(disabled ? QColor(100, 100, 100) : QColor(255, 255, 255), 3));
    p.drawArc(cx - radius, cy - radius, radius * 2, radius * 2,
              static_cast<int>((90.0 - m_angle) * 16),
              static_cast<int>(2 * m_angle * 16));

    p.setPen(QPen(disabled ? QColor(80, 80, 80) : QColor(100, 100, 100), 1));
    p.drawLine(srcL, srcR);

    p.setPen(QPen(disabled ? QColor(128, 128, 128) : QColor(200, 200, 200), 1));
    p.setBrush(QBrush(disabled ? QColor(100, 100, 100) : QColor(220, 220, 220)));
    p.drawRect(srcL.x() - 6, srcL.y() - 4, 12, 8);
    p.drawRect(srcR.x() - 6, srcR.y() - 4, 12, 8);

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(disabled ? QColor(60, 80, 120, 64) : QColor(100, 150, 255, 128)));
    QPointF apex(cx, cy);
    QPainterPath triPath;
    triPath.moveTo(srcL);
    triPath.lineTo(srcR);
    triPath.lineTo(apex);
    triPath.closeSubpath();
    p.drawPath(triPath);
}

void AzimuthSelector::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_anim->state() == QAbstractAnimation::Running) {
            m_angle = m_endAngle;
            updateSources();
            updateLabel();
            m_anim->stop();
        }

        m_pressStart = event->position().toPoint();
        m_dragStartAngle = m_angle;
        m_mousePressed = true;
        m_dragging = false;
    }
}

void AzimuthSelector::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mousePressed)
        return;

    QPoint pos = event->position().toPoint();
    QPoint pressStart(m_pressStart.x(), m_pressStart.y());
    QPoint delta = pos - pressStart;

    if (!m_dragging && delta.manhattanLength() > CLICK_MAX_PX) {
        m_dragging = true;
    }

    if (!m_dragging)
        return;

    m_anim->stop();

    double xDelta = static_cast<double>(delta.x());
    double yDelta = static_cast<double>(delta.y());
    double anglePerStep = m_maxAngle / DRAG_STEP_PX_BASE;
    double angleDelta = ((-yDelta + xDelta) / DRAG_STEP_PX_BASE) * m_maxAngle;
    double targetAngle = m_dragStartAngle + angleDelta;
    targetAngle = qBound(0.0, targetAngle, m_maxAngle);

    m_angle = targetAngle;
    updateSources();
    updateLabel();
    update();
}

void AzimuthSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_mousePressed)
        return;

    if (!m_dragging) {
        double stepAngle = 1.0;
        double targetAngle = m_angle + stepAngle;
        targetAngle = qBound(0.0, targetAngle, m_maxAngle);
        m_endAngle = targetAngle;
        m_angle = targetAngle;
        updateSources();
        updateLabel();
        update();
        animateToAngle(targetAngle, ANIMATION_DURATION_MS);
    } else {
        double snappedAngle = std::round(m_angle);
        m_endAngle = snappedAngle;
        m_angle = snappedAngle;
        updateSources();
        updateLabel();
        update();
        animateToAngle(snappedAngle, SNAP_DURATION_MS);
    }

    m_mousePressed = false;
    m_dragging = false;
}

void AzimuthSelector::updateLabel()
{
    m_label->setText(QString::number(static_cast<int>(m_angle)) + "°");
}

void AzimuthSelector::animateToAngle(double targetAngle, int durationMs)
{
    m_anim->stop();
    m_anim->setStartValue(m_angle);
    m_anim->setEndValue(targetAngle);
    m_anim->setDuration(durationMs);
    m_anim->start();
    connect(m_anim, &QPropertyAnimation::finished, this, [this, targetAngle]() {
        m_angle = targetAngle;
        updateSources();
        updateLabel();
        update();
        emit angleChanged(m_angle);
    });
}

void AzimuthSelector::wheelEvent(QWheelEvent *event)
{
    double angleDelta = static_cast<double>(event->angleDelta().y()) * 0.5;
    double targetAngle = m_angle + angleDelta;
    targetAngle = qBound(0.0, targetAngle, m_maxAngle);
    setAngle(targetAngle);
}

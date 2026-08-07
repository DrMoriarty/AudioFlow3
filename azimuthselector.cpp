#include "azimuthselector.h"

#include <QPainter>
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
    double sourceDist = 70.0;

    QPointF srcL(cx + static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));
    QPointF srcR(cx - static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));

    QLineF arcLine(srcL, srcR);
    QLineF triLine(srcL, srcR);

    int triSide = 60;
    double triHeight = static_cast<double>(triSide) * std::sqrt(3.0) / 2.0;

    QPainterPath path;
    path.moveTo(srcL);
    path.lineTo(srcR);
    path.lineTo(srcL);

    QPainterPath fillPath;
    fillPath.moveTo(srcL);
    fillPath.lineTo(srcR);
    fillPath.lineTo(QPointF(cx, cy - static_cast<int>(triHeight / 2.0)));

    m_arcPath = arcLine;
    m_triPath = fillPath;
    m_srcL = srcL;
    m_srcR = srcR;
    m_triSide = triSide;
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

    p.setPen(QPen(QColor(128, 128, 128), 1));
    p.drawEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

    double rad = degreesToRadians(m_angle);
    double sourceDist = 70.0;

    QPointF srcL(cx + static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));
    QPointF srcR(cx - static_cast<int>(sourceDist * qSin(rad)), cy - static_cast<int>(sourceDist * qCos(rad)));

    p.setPen(QPen(QColor(255, 255, 255), 3));
    p.drawArc(cx - radius, cy - radius, radius * 2, radius * 2,
              static_cast<int>(qDegreesToRadians(-90.0) * 16),
              static_cast<int>(qDegreesToRadians(90.0 - m_angle) * 16));

    p.setPen(QPen(QColor(100, 100, 100), 1));
    p.drawLine(srcL, srcR);

    p.setPen(QPen(QColor(200, 200, 200), 1));
    p.setBrush(QColor(220, 220, 220));
    p.drawRect(srcL.x() - 6, srcL.y() - 4, 12, 8);
    p.drawRect(srcR.x() - 6, srcR.y() - 4, 12, 8);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(200, 200, 200));
    double triHeight = static_cast<double>(m_triSide) * std::sqrt(3.0) / 2.0;
    QPointF apex(cx, cy - static_cast<int>(triHeight / 2.0));
    QPainterPath triPath;
    triPath.moveTo(srcL);
    triPath.lineTo(srcR);
    triPath.lineTo(apex);
    triPath.closeSubpath();
    p.drawPath(triPath);

    p.setBrush(QColor(100, 150, 255, 128));
    p.drawPath(m_triPath);
}

void AzimuthSelector::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressCenter = event->position().toPoint();
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
    QPoint delta = pos - m_pressCenter.toPoint();

    if (!m_dragging && delta.manhattanLength() > 5) {
        m_dragging = true;
    }

    if (!m_dragging)
        return;

    m_anim->stop();

    double angleDelta = static_cast<double>(delta.y()) * 0.05;
    double targetAngle = m_dragStartAngle + angleDelta;
    targetAngle = qBound(0.0, targetAngle, m_maxAngle);
    m_anim->setStartValue(m_angle);
    m_anim->setEndValue(targetAngle);
    m_anim->start();
}

void AzimuthSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_mousePressed)
        return;

    if (!m_dragging) {
        m_anim->stop();
        m_anim->setStartValue(m_angle);
        m_anim->setEndValue(m_angle);
        m_anim->start();
    }

    m_mousePressed = false;
    m_dragging = false;
}

void AzimuthSelector::wheelEvent(QWheelEvent *event)
{
    double angleDelta = static_cast<double>(event->angleDelta().y()) * 0.5;
    double targetAngle = m_angle + angleDelta;
    targetAngle = qBound(0.0, targetAngle, m_maxAngle);
    setAngle(targetAngle);
}

#include "ledindicator.h"
#include <QPainter>

LedIndicator::LedIndicator(QWidget *parent)
    : QWidget(parent)
    , m_color(colorForState(Transparent))
{
    setFixedSize(12, 12);
    m_animation = new QPropertyAnimation(this, "color", this);
    m_animation->setDuration(250);
    m_animation->setEasingCurve(QEasingCurve::Linear);
}

void LedIndicator::setState(State s, const QString &tooltip) {
    if (m_state == s) return;
    State prev = m_state;
    m_state = s;
    setToolTip(tooltip);

    m_animation->stop();
    m_animation->setStartValue(colorForState(prev));
    m_animation->setEndValue(colorForState(s));
    m_animation->start();
}

void LedIndicator::setColor(const QColor &c) {
    if (m_color == c) return;
    m_color = c;
    update();
}

QColor LedIndicator::colorForState(State s) {
    switch (s) {
        case Green:  return QColor(60, 200, 60);
        case Yellow: return QColor(220, 180, 30);
        case Red:    return QColor(220, 50, 50);
        default:     return QColor(80, 80, 80, 0);
    }
}

void LedIndicator::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(m_color);
    if (m_color.alpha() == 0) {
        p.setPen(QColor(80, 80, 80, 60));
        p.setBrush(QColor(80, 80, 80, 30));
    } else {
        p.setPen(QColor(40, 40, 40));
    }
    p.drawEllipse(1, 1, width() - 2, height() - 2);
}

#include "toggleswitch.h"

#include <QPainter>
#include <QPropertyAnimation>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent), m_checked(true), m_knobPos(20)
{
    setFixedSize(40, 22);
    setCursor(Qt::PointingHandCursor);

    m_anim = new QPropertyAnimation(this, "knobPos", this);
    m_anim->setDuration(150);
    m_anim->setEasingCurve(QEasingCurve::InOutCubic);
}

bool ToggleSwitch::isChecked() const
{
    return m_checked;
}

void ToggleSwitch::setChecked(bool checked)
{
    if (m_checked == checked) return;
    m_checked = checked;
    animateTo(checked);
    emit toggled(checked);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(40, 22);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = m_checked ? QColor(32, 102, 200) : QColor(32, 36, 40);
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, width(), height(), 11, 11);

    QColor knobColor(210, 220, 230);
    p.setBrush(knobColor);
    p.drawEllipse(m_knobPos, 2, 18, 18);
}

void ToggleSwitch::mousePressEvent(QMouseEvent *)
{
    m_checked = !m_checked;
    animateTo(m_checked);
    emit toggled(m_checked);
}

void ToggleSwitch::animateTo(bool checked)
{
    int target = checked ? 20 : 2;
    m_anim->stop();
    m_anim->setStartValue(m_knobPos);
    m_anim->setEndValue(target);
    m_anim->start();
}

int ToggleSwitch::knobPos() const
{
    return m_knobPos;
}

void ToggleSwitch::setKnobPos(int pos)
{
    m_knobPos = pos;
    update();
}

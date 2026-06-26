#include "knobwidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineF>
#include <QtMath>

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
        updateLabel();
        update();
    }
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

    double startAngle = 225.0;
    double endAngle = -45.0;
    if (m_values.size() > 1) {
        double ratio = static_cast<double>(m_index) / (m_values.size() - 1);
        double angle = startAngle + ratio * (endAngle - startAngle);
        double rad = qDegreesToRadians(angle);
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
        m_index = (m_index + 1) % m_values.size();
        updateLabel();
        update();
        emit valueChanged(currentValue());
    }
}

void KnobWidget::updateLabel()
{
    m_label->setText(m_values.value(m_index));
}

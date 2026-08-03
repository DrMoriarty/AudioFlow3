#include "knobwidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineF>
#include <QtMath>

static constexpr float CLICK_THRESHOLD = 16.0f;

static float distSquared(const QPoint &a, const QPoint &b)
{
    float dx = a.x() - b.x();
    float dy = a.y() - b.y();
    return dx * dx + dy * dy;
}

KnobWidget::KnobWidget(const QStringList &values, QWidget *parent)
    : QWidget(parent)
    , m_values(values)
    , m_index(0)
    , m_pressStart()
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
    QPoint pos = event->position().toPoint();
    
    if (event->button() == Qt::LeftButton) {
        // Calculate distance from previous press (0 on first press)
        float dx = pos.x() - m_pressStart.x();
        float dy = pos.y() - m_pressStart.y();
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist < CLICK_THRESHOLD) {
            // Fast click - cycle to next value
            m_index = (m_index + 1) % m_values.size();
            updateLabel();
            update();
            emit valueChanged(currentValue());
            m_mousePressed = false;
            m_pressStart = QPoint();
            m_switchCount = 0;
            return;
        }
        
        // Start dragging
        m_mousePressed = true;
        m_pressStart = pos;
        m_dragging = false;
        m_switchCount = 0;
    } else if (event->button() == Qt::RightButton) {
        // Right click - cycle to previous value
        m_index = (m_index - 1 + m_values.size()) % m_values.size();
        updateLabel();
        update();
        m_mousePressed = false;
        m_pressStart = QPoint();
        return;
    }
}

void KnobWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_mousePressed) {
        int yDelta = event->position().y() - m_pressStart.y();
        
        // Check if we've moved enough to be considered a drag
        float dist = (event->position().toPoint() - m_pressStart).manhattanLength();
        if (dist > CLICK_THRESHOLD) {
            m_dragging = true;
        }
        
        // Threshold of 60 pixels for slower switching
        int threshold = 60;
        
        // Calculate how many times we should have switched based on distance
        int newSwitchCount = yDelta / threshold;
        
        // Switch when the count changes
        if (newSwitchCount != m_switchCount) {
            int switches = newSwitchCount - m_switchCount;
            
            // yDelta < 0 → moving up → next value (forward)
            // yDelta > 0 → moving down → previous value (backward)
            if (switches > 0) {
                m_index = (m_index - switches + m_values.size()) % m_values.size();
            } else if (switches < 0) {
                m_index = (m_index - switches) % m_values.size();
            }
            
            m_switchCount = newSwitchCount;
            updateLabel();
            update();
        }
    }
}

void KnobWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_mousePressed && m_switchCount == 0 && !m_dragging) {
        // Fast click — toggle to next value on release
        m_index = (m_index + 1) % m_values.size();
        updateLabel();
        update();
        emit valueChanged(currentValue());
        m_mousePressed = false;
        m_pressStart = QPoint();
        m_switchCount = 0;
        m_dragging = false;
    } else if (event->button() == Qt::LeftButton && m_mousePressed && m_switchCount != 0) {
        // Drag released — keep current value
        m_mousePressed = false;
        m_pressStart = QPoint();
        m_dragging = false;
    }
}

void KnobWidget::updateLabel()
{
    m_label->setText(m_values.value(m_index));
}

void KnobWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QMouseEvent *clickEvent = event;
    emit valueChanged(currentValue());
}

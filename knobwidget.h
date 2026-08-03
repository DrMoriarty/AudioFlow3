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

    QStringList m_values;
    int m_index;
    QLabel *m_label;
    bool m_mousePressed = false;
    bool m_dragging = false;
    QPoint m_pressStart;
    int m_switchCount = 0;
    static constexpr float CLICK_THRESHOLD = 16.0f;
};

#endif // KNOBWIDGET_H

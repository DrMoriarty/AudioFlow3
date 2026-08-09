#ifndef LEDINDICATOR_H
#define LEDINDICATOR_H

#include <QWidget>
#include <QColor>
#include <QPropertyAnimation>

class LedIndicator : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)

public:
    enum State { Transparent, Green, Yellow, Red };

    explicit LedIndicator(QWidget *parent = nullptr);

    void setState(State s, const QString &tooltip = {});
    State state() const { return m_state; }

    QColor color() const { return m_color; }
    void setColor(const QColor &c);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    static QColor colorForState(State s);

    State m_state = Transparent;
    QColor m_color;
    QPropertyAnimation *m_animation = nullptr;
};

#endif // LEDINDICATOR_H

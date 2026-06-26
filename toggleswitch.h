#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QWidget>

class QPropertyAnimation;

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int knobPos READ knobPos WRITE setKnobPos)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);
    int knobPos() const;
    void setKnobPos(int pos);

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    QSize sizeHint() const override;

private:
    bool m_checked;
    int m_knobPos;
    QPropertyAnimation *m_anim;

    void animateTo(bool checked);
};

#endif // TOGGLESWITCH_H

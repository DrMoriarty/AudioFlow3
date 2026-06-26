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

private:
    void updateLabel();

    QStringList m_values;
    int m_index;
    QLabel *m_label;
};

#endif // KNOBWIDGET_H

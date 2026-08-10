#ifndef COLLAPSIBLEBLOCK_H
#define COLLAPSIBLEBLOCK_H

#include <QFrame>

class QPushButton;
class QLabel;
class QVBoxLayout;
class QWidget;
class ToggleSwitch;
class LedIndicator;
class KnobWidget;

class CollapsibleBlock : public QFrame
{
    Q_OBJECT

public:
    explicit CollapsibleBlock(const QString &title, QWidget *parent = nullptr);

    void setContentWidget(QWidget *widget);
    void setExpanded(bool expanded);
    bool isExpanded() const;
    QWidget *headerWidget() const;
    QWidget *contentWidget() const;
    int headerHeight() const;
    void recalcHeaderHeight();
    void addToggleSwitch();
    void setToggleChecked(bool checked);
    bool isToggleChecked() const;
    LedIndicator *addClipLed();
    KnobWidget *addPreAmpKnob(double min, double max, double step, double value, const QString &suffix);

signals:
    void expandedChanged();
    void toggled(bool checked);

private slots:
    void toggle();

private:
    void updateChevron();
    void updateHeaderColor();

    QPushButton *m_toggleButton;
    QLabel *m_titleLabel;
    QWidget *m_headerWidget;
    QWidget *m_contentWidget;
    QWidget *m_leftSpacer;
    QVBoxLayout *m_mainLayout;
    ToggleSwitch *m_toggleSwitch = nullptr;
    bool m_expanded;
    int m_headerHeight = 0;
    static const int m_borderWidth = 4;
};

#endif // COLLAPSIBLEBLOCK_H

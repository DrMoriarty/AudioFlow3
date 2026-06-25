#ifndef COLLAPSIBLEBLOCK_H
#define COLLAPSIBLEBLOCK_H

#include <QFrame>

class QPushButton;
class QLabel;
class QVBoxLayout;
class QWidget;

class CollapsibleBlock : public QFrame
{
    Q_OBJECT

public:
    explicit CollapsibleBlock(const QString &title, QWidget *parent = nullptr);

    void setContentWidget(QWidget *widget);
    void setExpanded(bool expanded);
    bool isExpanded() const;

signals:
    void expandedChanged();

private slots:
    void toggle();

private:
    void updateChevron();

    QPushButton *m_toggleButton;
    QLabel *m_titleLabel;
    QWidget *m_contentWidget;
    QVBoxLayout *m_mainLayout;
    bool m_expanded;
};

#endif // COLLAPSIBLEBLOCK_H

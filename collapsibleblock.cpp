#include "collapsibleblock.h"

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QLayout>

CollapsibleBlock::CollapsibleBlock(const QString &title, QWidget *parent)
    : QFrame(parent), m_expanded(false)
{
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(
        "CollapsibleBlock {"
        "  border: 2px solid #606468;"
        "  border-radius: 8px;"
        "  background-color: #202428;"
        "}"
    );

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    QWidget *headerWidget = new QWidget(this);
    headerWidget->setObjectName("headerWidget");
    m_headerWidget = headerWidget;
    headerWidget->setStyleSheet(
        "QWidget#headerWidget {"
        "  border-bottom: 2px solid #606468;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "  background-color: #303438;"
        "}"
    );
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(5, 5, 5, 5);

    m_toggleButton = new QPushButton("▶", headerWidget);
    m_toggleButton->setFixedSize(20, 20);
    m_toggleButton->setStyleSheet(
        "QPushButton {"
        "  border: none;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #606468;"
        "  border-radius: 3px;"
        "}"
    );
    connect(m_toggleButton, &QPushButton::clicked, this, &CollapsibleBlock::toggle);

    m_titleLabel = new QLabel(title, headerWidget);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_titleLabel->font();
    font.setBold(true);
    m_titleLabel->setFont(font);

    headerLayout->addWidget(m_toggleButton);
    headerLayout->addStretch();
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addStretch();

    QWidget *spacer = new QWidget(headerWidget);
    spacer->setFixedSize(20, 20);
    headerLayout->addWidget(spacer);

    m_mainLayout->addWidget(headerWidget);

    m_contentWidget = new QWidget(this);
    m_contentWidget->setMinimumHeight(30);
    m_contentWidget->setVisible(false);
    m_contentWidget->setStyleSheet(
        "QWidget {"
        "  border-bottom-left-radius: 7px;"
        "  border-bottom-right-radius: 7px;"
        "  background-color: #202428;"
        "}"
    );
    m_mainLayout->addWidget(m_contentWidget);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    headerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void CollapsibleBlock::setContentWidget(QWidget *widget)
{
    if (m_contentWidget) {
        m_mainLayout->removeWidget(m_contentWidget);
        m_contentWidget->deleteLater();
    }

    m_contentWidget = widget;
    m_contentWidget->setVisible(m_expanded);
    m_mainLayout->addWidget(m_contentWidget);
}

void CollapsibleBlock::setExpanded(bool expanded)
{
    m_expanded = expanded;
    if (m_contentWidget) {
        m_contentWidget->setVisible(m_expanded);
    }
    updateChevron();
    emit expandedChanged();
}

bool CollapsibleBlock::isExpanded() const
{
    return m_expanded;
}

void CollapsibleBlock::toggle()
{
    setExpanded(!m_expanded);
}

void CollapsibleBlock::updateChevron()
{
    m_toggleButton->setText(m_expanded ? "▼" : "▶");
}

QWidget *CollapsibleBlock::headerWidget() const
{
    return m_headerWidget;
}

QWidget *CollapsibleBlock::contentWidget() const
{
    return m_contentWidget;
}

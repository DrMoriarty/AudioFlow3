#include "collapsibleblock.h"
#include "toggleswitch.h"

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QLayout>

CollapsibleBlock::CollapsibleBlock(const QString &title, QWidget *parent)
    : QFrame(parent), m_expanded(false)
{
    setFrameShape(QFrame::NoFrame);
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

    m_leftSpacer = new QWidget(headerWidget);
    m_leftSpacer->setFixedSize(14, 22);
    m_leftSpacer->setStyleSheet("background: transparent; border: none;");
    headerLayout->addWidget(m_leftSpacer);
    headerLayout->addWidget(m_toggleButton);
    headerLayout->addStretch();
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addStretch();

    QWidget *spacer = new QWidget(headerWidget);
    spacer->setObjectName("headerRightSpacer");
    spacer->setFixedSize(40, 22);
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

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    headerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_headerHeight = headerWidget->sizeHint().height() + 4; // +4 for QFrame border
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

int CollapsibleBlock::headerHeight() const
{
    return m_headerHeight;
}

void CollapsibleBlock::addToggleSwitch()
{
    QWidget *spacer = m_headerWidget->findChild<QWidget *>("headerRightSpacer");
    QLayout *headerLayout = m_headerWidget->layout();
    if (spacer) {
        headerLayout->removeWidget(spacer);
        spacer->deleteLater();
    }

    m_toggleSwitch = new ToggleSwitch(m_headerWidget);
    m_toggleSwitch->setChecked(true);
    connect(m_toggleSwitch, &ToggleSwitch::toggled, this, &CollapsibleBlock::toggled);
    connect(m_toggleSwitch, &ToggleSwitch::toggled, this, [this](bool) { updateHeaderColor(); });
    static_cast<QBoxLayout *>(headerLayout)->addWidget(m_toggleSwitch, 0, Qt::AlignVCenter);
    updateHeaderColor();
}

void CollapsibleBlock::setToggleChecked(bool checked)
{
    if (m_toggleSwitch)
        m_toggleSwitch->setChecked(checked);
    updateHeaderColor();
}

void CollapsibleBlock::updateHeaderColor()
{
    const int shift = 15;
    bool on = m_toggleSwitch && m_toggleSwitch->isChecked();
    int r = on ? 48 + shift : 48;
    int g = on ? 52 + shift : 52;
    int b = on ? 56 + shift : 56;
    m_headerWidget->setStyleSheet(QStringLiteral(
        "QWidget#headerWidget {"
        "  border-bottom: 2px solid #606468;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "  background-color: rgb(%1, %2, %3);"
        "}"
    ).arg(r).arg(g).arg(b));

    const int base = 224;
    int tr = on ? base : base - 50;
    int tg = on ? base : base - 50;
    int tb = on ? base : base - 50;
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: rgb(%1, %2, %3); font-weight: bold; }"
    ).arg(tr).arg(tg).arg(tb));
}

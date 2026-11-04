#include "ChatBubble.h"

#include "AvatarWidget.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>

ChatBubble::ChatBubble(const QString &role, const QString &text, const QString &avatar,
                       const QColor &accent, QWidget *parent)
    : QWidget(parent)
    , m_assistant(role == QStringLiteral("assistant"))
{
    setAttribute(Qt::WA_StyledBackground, false);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(9);

    m_label = new QLabel(text.isEmpty() ? QStringLiteral(" ") : text, this);
    m_label->setWordWrap(true);
    m_label->setTextFormat(Qt::PlainText);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_label->setMaximumWidth(570);
    m_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    if (m_assistant) {
        auto *avatarWidget = new AvatarWidget(avatar, accent, 34, this);
        row->addWidget(avatarWidget, 0, Qt::AlignTop);
        row->addWidget(m_label, 0, Qt::AlignTop);
        row->addStretch(1);
        m_label->setStyleSheet(QStringLiteral(
            "background: #FFFFFF; color: #24332F; border: 1px solid #E9E1D5;"
            "border-radius: 17px; padding: 11px 15px; font-size: 14px;"));
    } else {
        row->addStretch(1);
        row->addWidget(m_label, 0, Qt::AlignTop);
        m_label->setStyleSheet(QStringLiteral(
            "background: #173F38; color: #F8F3EA; border: 1px solid #173F38;"
            "border-radius: 17px; padding: 11px 15px; font-size: 14px;"));
    }

    updateWidth();
}

void ChatBubble::updateWidth()
{
    const QString content = m_label->text();
    QFont metricsFont = m_label->font();
    metricsFont.setPixelSize(14);
    const QFontMetrics fm(metricsFont);

    int maxLine = 0;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines)
        maxLine = qMax(maxLine, fm.horizontalAdvance(line));

    const int width = qBound(200, maxLine + 40, 560);
    m_label->setMinimumWidth(width);
    m_label->setMaximumWidth(560);
}

void ChatBubble::setText(const QString &text)
{
    m_label->setText(text.isEmpty() ? QStringLiteral(" ") : text);
    updateWidth();
    m_label->adjustSize();
    updateGeometry();
}

QString ChatBubble::text() const
{
    return m_label->text();
}

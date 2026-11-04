#include "AvatarWidget.h"

#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>

AvatarWidget::AvatarWidget(QWidget *parent)
    : AvatarWidget(QStringLiteral("✨"), QColor(46, 110, 99), 46, parent)
{
}

AvatarWidget::AvatarWidget(const QString &emoji, const QColor &accent, int diameter, QWidget *parent)
    : QWidget(parent)
    , m_emoji(emoji)
    , m_accent(accent)
    , m_diameter(diameter)
{
    setFixedSize(m_diameter, m_diameter);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void AvatarWidget::setEmoji(const QString &emoji)
{
    m_emoji = emoji;
    update();
}

void AvatarWidget::setAccent(const QColor &accent)
{
    m_accent = accent;
    update();
}

void AvatarWidget::setDiameter(int diameter)
{
    m_diameter = diameter;
    setFixedSize(m_diameter, m_diameter);
    update();
}

void AvatarWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0.0, m_accent.lighter(132));
    g.setColorAt(1.0, m_accent.darker(112));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(r);

    p.setBrush(QColor(255, 255, 255, 28));
    p.drawEllipse(QRectF(r.left() + r.width() * 0.16, r.top() + r.height() * 0.12,
                         r.width() * 0.46, r.height() * 0.28));

    QFont f(QStringLiteral("Segoe UI Emoji"));
    f.setPixelSize(qMax(12, int(m_diameter * 0.46)));
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(rect(), Qt::AlignCenter, m_emoji);
}

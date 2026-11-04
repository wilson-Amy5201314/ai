#include "TypingIndicator.h"

#include <QPainter>
#include <QRadialGradient>
#include <QTimer>
#include <QtMath>

TypingIndicator::TypingIndicator(const QColor &accent, QWidget *parent)
    : QWidget(parent)
    , m_accent(accent)
{
    setFixedSize(92, 40);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    m_timer = new QTimer(this);
    m_timer->setInterval(360);
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_phase = (m_phase + 1) % 3;
        update();
    });
    m_timer->start();
}

void TypingIndicator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF avatar(1, 5, 30, 30);
    QRadialGradient g(avatar.center(), 18);
    g.setColorAt(0.0, m_accent.lighter(135));
    g.setColorAt(1.0, m_accent.darker(108));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(avatar);

    QFont ef(QStringLiteral("Segoe UI Emoji"));
    ef.setPixelSize(13);
    p.setFont(ef);
    p.setPen(QColor(255, 255, 255));
    p.drawText(avatar, Qt::AlignCenter, QStringLiteral("…"));

    const QRectF bubble(40, 3, 50, 34);
    p.setBrush(QColor(255, 255, 255));
    p.setPen(QPen(QColor(233, 225, 213), 1.0));
    p.drawRoundedRect(bubble, 16, 16);

    for (int i = 0; i < 3; ++i) {
        const int alpha = (i == m_phase) ? 220 : 92;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(46, 110, 99, alpha));
        p.drawEllipse(QPointF(56 + i * 9, 20), 3.0, 3.0);
    }
}

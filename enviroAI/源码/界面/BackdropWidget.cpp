#include "BackdropWidget.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

BackdropWidget::BackdropWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
}

void BackdropWidget::paintEvent(QPaintEvent *)
{
    const qreal dpr = devicePixelRatioF();
    if (m_cache.isNull() || m_cacheSize != size() || m_cacheDpr != dpr)
        rebuildCache();
    if (m_cache.isNull())
        return;
    QPainter p(this);
    p.drawPixmap(QPointF(0, 0), m_cache);
}

void BackdropWidget::rebuildCache()
{
    const qreal dpr = devicePixelRatioF();
    const QSize logical = size();
    if (logical.isEmpty())
        return;

    QPixmap pixmap(qMax(1, qRound(logical.width() * dpr)),
                   qMax(1, qRound(logical.height() * dpr)));
    pixmap.setDevicePixelRatio(dpr);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r(0, 0, logical.width(), logical.height());
    QLinearGradient base(r.topLeft(), r.bottomRight());
    base.setColorAt(0.0, QColor(249, 244, 234));
    base.setColorAt(0.45, QColor(246, 240, 230));
    base.setColorAt(1.0, QColor(226, 240, 233));
    p.fillRect(r, base);

    QRadialGradient warm(r.width() * 0.10, r.height() * 0.08, r.width() * 0.55);
    warm.setColorAt(0.0, QColor(236, 203, 174, 120));
    warm.setColorAt(0.55, QColor(236, 203, 174, 34));
    warm.setColorAt(1.0, QColor(236, 203, 174, 0));
    p.fillRect(r, warm);

    QRadialGradient mint(r.width() * 0.92, r.height() * 0.90, r.width() * 0.50);
    mint.setColorAt(0.0, QColor(129, 190, 173, 90));
    mint.setColorAt(0.55, QColor(129, 190, 173, 26));
    mint.setColorAt(1.0, QColor(129, 190, 173, 0));
    p.fillRect(r, mint);

    QRadialGradient gold(r.width() * 0.80, r.height() * 0.12, r.width() * 0.24);
    gold.setColorAt(0.0, QColor(221, 185, 124, 50));
    gold.setColorAt(1.0, QColor(221, 185, 124, 0));
    p.fillRect(r, gold);

    // 低饱和的“水纹”线条，给背景一点手绘感。
    p.save();
    p.setPen(QPen(QColor(46, 110, 99, 18), 1.1));
    p.setBrush(Qt::NoBrush);
    const QPointF c(r.width() * 0.07, r.height() * 0.92);
    for (int i = 1; i <= 5; ++i) {
        const qreal radius = 150.0 + i * 62.0;
        p.drawArc(QRectF(c.x() - radius, c.y() - radius, radius * 2, radius * 2),
                  25 * 16, 105 * 16);
    }
    p.restore();

    // 右上角的小金点，像远处的灯。
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(216, 174, 108, 120));
    p.drawEllipse(QPointF(r.width() - 92, 74), 4.2, 4.2);
    p.setBrush(QColor(216, 174, 108, 42));
    p.drawEllipse(QPointF(r.width() - 92, 74), 12.0, 12.0);

    p.end();
    m_cache = pixmap;
    m_cacheSize = logical;
    m_cacheDpr = dpr;
}

#pragma once

#include <QPixmap>
#include <QSize>
#include <QWidget>

class BackdropWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BackdropWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuildCache();

    QPixmap m_cache;
    QSize m_cacheSize;
    qreal m_cacheDpr = 0.0;
};

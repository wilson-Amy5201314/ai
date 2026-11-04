#pragma once

#include <QColor>
#include <QWidget>

class AvatarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AvatarWidget(QWidget *parent = nullptr);
    AvatarWidget(const QString &emoji, const QColor &accent, int diameter,
                 QWidget *parent = nullptr);

    void setEmoji(const QString &emoji);
    void setAccent(const QColor &accent);
    void setDiameter(int diameter);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_emoji;
    QColor m_accent;
    int m_diameter;
};

#pragma once

#include <QColor>
#include <QWidget>

class QTimer;

class TypingIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit TypingIndicator(const QColor &accent, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_accent;
    QTimer *m_timer = nullptr;
    int m_phase = 0;
};

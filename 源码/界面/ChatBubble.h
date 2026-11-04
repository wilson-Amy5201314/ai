#pragma once

#include <QColor>
#include <QWidget>

class QLabel;

class ChatBubble : public QWidget
{
    Q_OBJECT
public:
    ChatBubble(const QString &role, const QString &text, const QString &avatar,
               const QColor &accent, QWidget *parent = nullptr);

    void setText(const QString &text);
    QString text() const;
    bool isAssistant() const { return m_assistant; }

private:
    void updateWidth();

    QLabel *m_label = nullptr;
    bool m_assistant = false;
};

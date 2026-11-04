#pragma once

#include "Models.h"

#include <QWidget>

namespace Ui {
class ChatPage;
}

class ChatBubble;
class TypingIndicator;
class QTimer;

class ChatPage : public QWidget
{
    Q_OBJECT
public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage() override;

    void begin(const Persona &persona);
    void addBubble(const ChatTurn &turn);
    void beginAssistantStream();
    void appendAssistantDelta(const QString &delta);
    void finishAssistantStream(const QString &fullText);
    void failAssistant(const QString &error);
    void setBusy(bool busy);
    void setMemoryStatus(bool online, int pending);
    void clearConversation();

signals:
    void backRequested();
    void sendRequested(const QString &text);
    void restartRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void sendCurrent();
    void scrollToBottom();
    void showTyping(bool show);
    void showEmptyState(bool visible);
    void addNotice(const QString &text);
    void clearMessageWidgets();
    void refreshMemoryStyle();

    Ui::ChatPage *ui = nullptr;
    Persona m_persona;
    TypingIndicator *m_typing = nullptr;
    ChatBubble *m_streamingBubble = nullptr;
    QTimer *m_streamTimer = nullptr;
    QString m_streamingText;
    bool m_busy = false;
};

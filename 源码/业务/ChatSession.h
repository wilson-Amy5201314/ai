#pragma once

#include "Models.h"

#include <QJsonArray>
#include <QObject>
#include <QVector>

class AiClient;
class Database;

class ChatSession : public QObject
{
    Q_OBJECT
public:
    ChatSession(const Persona &persona, const QString &userGender, const QString &userName,
                Database *db, AiClient *ai, QObject *parent = nullptr);

    void start();
    void restore(const QString &sessionId, const QVector<ChatTurn> &turns);
    void send(const QString &text);
    void abort();

    QString id() const { return m_id; }

signals:
    void messageAdded(const ChatTurn &turn);
    void assistantStarted();
    void assistantDelta(const QString &delta);
    void assistantFinished(const QString &fullText);
    void assistantFailed(const QString &friendlyError);
    void busyChanged(bool busy);

private:
    QString systemPrompt() const;
    QJsonArray buildMessages() const;
    void addTurn(const QString &role, const QString &content, bool persist = true);

    Persona m_persona;
    QString m_gender;
    QString m_userName;
    QString m_id;
    Database *m_db = nullptr;
    AiClient *m_ai = nullptr;
    QVector<ChatTurn> m_turns;
    QDateTime m_startedAt;
    bool m_busy = false;
    bool m_started = false;
};

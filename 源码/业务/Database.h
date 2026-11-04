#pragma once

#include "Models.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    bool open();
    void close();
    bool isOnline() const { return m_online; }

    // 用户资料（性别 + 年龄）与用户自定义角色。
    UserProfile ensureProfile(const QString &gender, int age, QString *error = nullptr);
    UserProfile ensureProfile(const QString &gender, int age, const QString &name,
                              QString *error = nullptr);
    QVector<UserProfile> profiles(int limit = 5);
    UserProfile profileById(const QString &profileId);
    int profileCount();
    bool deleteProfile(const QString &profileId);
    bool deleteRole(const QString &roleId);
    bool deleteSession(const QString &sessionId);
    QVector<CustomRole> rolesForProfile(const QString &profileId, int limit = 5);
    bool insertRole(const CustomRole &role);
    bool setRoleName(const QString &roleId, const QString &name);
    CustomRole roleById(const QString &roleId);
    QVector<SessionInfo> sessionsForRole(const QString &roleId, int limit = 10);
    bool setSessionLocked(const QString &sessionId, bool locked);
    QVector<ChatTurn> messagesForSession(const QString &sessionId);

    bool saveSession(const QString &id, const QString &gender, const QString &roleKey,
                     const QString &personaKey, const QString &startedAt = QString());
    bool saveMessage(const QString &sessionId, const QString &sender, const QString &content);

    int pendingCount() const { return m_pendingSessions.size() + m_pendingMessages.size(); }
    bool flushPending();

    // 轻量长期记忆：只记偏好、最近在忙什么、在意的小事，不记姓名。
    bool storeFacts(const QString &personaKey, const QString &userText);
    QStringList facts(const QString &personaKey, int limit = 6) const;

signals:
    void statusChanged(bool online, const QString &detail);
    void pendingFlushed(int count);

private:
    struct PendingSession {
        QString id;
        QString gender;
        QString roleKey;
        QString personaKey;
        QString startedAt;
    };
    struct PendingMessage {
        QString sessionId;
        QString sender;
        QString content;
    };

    bool ensureSchema();
    bool ensureSchemaFromResource();
    bool ping();
    bool insertSessionNow(const QString &id, const QString &gender, const QString &roleKey,
                          const QString &personaKey, const QString &startedAt);
    bool insertMessageNow(const QString &sessionId, const QString &sender, const QString &content);
    void queueSession(const PendingSession &s);
    void queueMessage(const PendingMessage &m);
    void loadPending();
    bool savePending();

    QSqlDatabase m_db;
    bool m_online = false;
    QString m_lastError;
    QVector<PendingSession> m_pendingSessions;
    QVector<PendingMessage> m_pendingMessages;
};

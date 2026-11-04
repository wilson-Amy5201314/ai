#pragma once

#include "Models.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkReply;

class AiClient : public QObject
{
    Q_OBJECT
public:
    explicit AiClient(QObject *parent = nullptr);

    void sendChat(const QJsonArray &messages);
    void generateCustomRole(const RoleRequest &request);
    void abort();
    bool busy() const { return m_reply != nullptr; }

signals:
    void streamStarted();
    void streamDelta(const QString &delta);
    void streamFinished(const QString &fullText);
    void streamFailed(const QString &friendlyError);

    void customRoleReady(const QJsonObject &role);
    void customRoleFailed(const QString &friendlyError);

private:
    QNetworkRequest makeRequest(const QString &accept) const;
    void startReply(QNetworkReply *reply);
    void onReadyRead();
    void onFinished();
    void processStreamBuffer();
    void finishStream(bool ok, const QString &error);
    void finishCustom(bool ok, const QString &error);
    void cleanupReply();
    static QString friendlyHttpError(int status, const QByteArray &body);

    QNetworkAccessManager m_nam;
    QNetworkReply *m_reply = nullptr;
    QTimer m_timeout;
    QByteArray m_buffer;
    QString m_full;
    bool m_streamMode = false;
    bool m_finished = false;
    bool m_gotAny = false;
    int m_status = 0;
};

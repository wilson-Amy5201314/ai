#include "AiClient.h"

#include "AppConfig.h"
#include "Prompts.h"

#include <QDebug>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
QString cleanJsonFence(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QStringLiteral("```"))) {
        const int firstNewline = text.indexOf(QLatin1Char('\n'));
        if (firstNewline >= 0)
            text = text.mid(firstNewline + 1);
        if (text.endsWith(QStringLiteral("```")))
            text.chop(3);
    }
    return text.trimmed();
}
} // namespace

AiClient::AiClient(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (!m_reply)
            return;
        qWarning() << "AI request timed out";
        if (m_streamMode)
            finishStream(false, QStringLiteral("这次等得有点久，稍后再试一次吧。"));
        else
            finishCustom(false, QStringLiteral("这次等得有点久，稍后再试一次吧。"));
    });
}

QNetworkRequest AiClient::makeRequest(const QString &accept) const
{
    const AppConfig &cfg = AppConfig::instance();
    QNetworkRequest req(QUrl(cfg.apiUrl));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + cfg.apiKey.toUtf8());
    req.setRawHeader("Accept", accept.toUtf8());
    req.setRawHeader("User-Agent", "XinYu/1.0 (Qt)");
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    return req;
}

void AiClient::sendChat(const QJsonArray &messages)
{
    if (m_reply)
        abort();

    const AppConfig &cfg = AppConfig::instance();

    QJsonObject body;
    body.insert(QStringLiteral("model"), cfg.model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), true);
    body.insert(QStringLiteral("temperature"), 1.0);
    body.insert(QStringLiteral("top_p"), 0.95);
    body.insert(QStringLiteral("max_tokens"), 900);
    body.insert(QStringLiteral("frequency_penalty"), 0.1);
    body.insert(QStringLiteral("presence_penalty"), 0.1);

    m_streamMode = true;
    m_finished = false;
    m_gotAny = false;
    m_full.clear();
    m_buffer.clear();
    m_status = 0;

    QNetworkReply *reply = m_nam.post(makeRequest(QStringLiteral("text/event-stream")),
                                      QJsonDocument(body).toJson(QJsonDocument::Compact));
    startReply(reply);
    emit streamStarted();
}

void AiClient::generateCustomRole(const RoleRequest &request)
{
    if (m_reply)
        abort();

    const AppConfig &cfg = AppConfig::instance();
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                {QStringLiteral("content"),
                                 QStringLiteral("你是一位资深的中文角色扮演与人物设定设计师。只输出合法 JSON，不要输出解释。")}});
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), customRolePrompt(request)}});

    QJsonObject body;
    body.insert(QStringLiteral("model"), cfg.model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), false);
    body.insert(QStringLiteral("response_format"),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("json_object")}});
    body.insert(QStringLiteral("temperature"), 1.0);
    body.insert(QStringLiteral("max_tokens"), 1400);

    m_streamMode = false;
    m_finished = false;
    m_gotAny = false;
    m_full.clear();
    m_buffer.clear();
    m_status = 0;

    QNetworkReply *reply = m_nam.post(makeRequest(QStringLiteral("application/json")),
                                      QJsonDocument(body).toJson(QJsonDocument::Compact));
    startReply(reply);
}

void AiClient::startReply(QNetworkReply *reply)
{
    m_reply = reply;
    m_status = 0;
    m_timeout.start(90000);

    connect(reply, &QNetworkReply::readyRead, this, &AiClient::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &AiClient::onFinished);
}

void AiClient::onReadyRead()
{
    if (!m_reply)
        return;
    if (!m_streamMode) {
        // 非流式响应留给 finished 时一次读取。
        return;
    }
    m_buffer += m_reply->readAll();
    processStreamBuffer();
}

void AiClient::processStreamBuffer()
{
    while (true) {
        const int newline = m_buffer.indexOf('\n');
        if (newline < 0)
            break;
        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (!line.startsWith("data:"))
            continue;

        const QByteArray payload = line.mid(5).trimmed();
        if (payload == "[DONE]") {
            finishStream(true, QString());
            return;
        }

        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;
        const QJsonObject root = doc.object();
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty())
            continue;
        const QJsonObject delta = choices.first().toObject().value(QStringLiteral("delta")).toObject();
        const QString content = delta.value(QStringLiteral("content")).toString();
        if (content.isEmpty())
            continue;
        m_full += content;
        m_gotAny = true;
        emit streamDelta(content);
    }
}

void AiClient::onFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_reply)
        return;

    m_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray rest = reply->readAll();
    const QNetworkReply::NetworkError netError = reply->error();

    if (!m_streamMode) {
        if (m_status >= 200 && m_status < 300) {
            QJsonParseError err{};
            const QJsonDocument doc = QJsonDocument::fromJson(rest, &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
                if (!choices.isEmpty()) {
                    const QJsonObject msg = choices.first().toObject().value(QStringLiteral("message")).toObject();
                    const QString content = cleanJsonFence(msg.value(QStringLiteral("content")).toString());
                    const QJsonDocument parsed = QJsonDocument::fromJson(content.toUtf8());
                    if (parsed.isObject()) {
                        m_full = content;
                        finishCustom(true, QString());
                        return;
                    }
                    qWarning() << "AI JSON parse failed:" << content.left(200);
                }
            }
            finishCustom(false, QStringLiteral("这次没能生成这个角色，再试一次吧。"));
            return;
        }
        finishCustom(false, friendlyHttpError(m_status, rest));
        return;
    }

    if (!rest.isEmpty()) {
        m_buffer += rest;
        processStreamBuffer();
        if (!m_reply)
            return;
    }

    if (m_gotAny) {
        finishStream(true, QString());
        return;
    }

    if (netError == QNetworkReply::OperationCanceledError) {
        finishStream(false, QStringLiteral("连接被中断了，再试一次吧。"));
        return;
    }
    if (netError != QNetworkReply::NoError && m_status == 0) {
        finishStream(false, QStringLiteral("网络好像走神了，检查一下连接再试。"));
        return;
    }
    finishStream(false, friendlyHttpError(m_status, rest));
}

QString AiClient::friendlyHttpError(int status, const QByteArray &body)
{
    QJsonObject errObj;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject())
        errObj = doc.object().value(QStringLiteral("error")).toObject();
    const QString detail = errObj.value(QStringLiteral("message")).toString();
    if (!detail.isEmpty())
        qWarning() << "AI HTTP" << status << detail;

    if (status == 401 || status == 403)
        return QStringLiteral("连接配置需要更新，暂时没法继续对话。");
    if (status == 429)
        return QStringLiteral("现在有点拥挤，等一小会儿再聊。");
    if (status >= 500)
        return QStringLiteral("对面暂时没有回应，过一会儿再试试。");
    if (status == 0)
        return QStringLiteral("网络好像走神了，检查一下连接再试。");
    return QStringLiteral("刚才没能接上话，再试一次吧。");
}

void AiClient::finishStream(bool ok, const QString &error)
{
    if (m_finished)
        return;
    m_finished = true;
    m_timeout.stop();
    cleanupReply();
    if (ok)
        emit streamFinished(m_full);
    else
        emit streamFailed(error);
}

void AiClient::finishCustom(bool ok, const QString &error)
{
    if (m_finished)
        return;
    m_finished = true;
    m_timeout.stop();

    QJsonObject role;
    if (ok) {
        const QJsonDocument doc = QJsonDocument::fromJson(m_full.toUtf8());
        if (doc.isObject())
            role = doc.object();
        if (role.value(QStringLiteral("system_prompt")).toString().trimmed().isEmpty())
            ok = false;
    }

    cleanupReply();
    if (ok)
        emit customRoleReady(role);
    else
        emit customRoleFailed(error.isEmpty() ? QStringLiteral("这次没能生成这个角色。") : error);
}

void AiClient::cleanupReply()
{
    if (!m_reply)
        return;
    m_reply->disconnect(this);
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
}

void AiClient::abort()
{
    if (!m_reply)
        return;
    m_finished = true;
    m_timeout.stop();
    cleanupReply();
}

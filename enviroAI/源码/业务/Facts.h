#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// 轻量事实抽取：只抓能长期帮助对话的偏好与小习惯，不抓姓名。
// 加规则 = 往 rules 里加一行 {正则, 模板}。
inline QStringList extractFacts(const QString &userText)
{
    QStringList found;
    static const struct {
        const char *pattern;
        const char *fmt;
    } rules[] = {
        {u8"我喜欢([^。！？!?\\n]{1,20})", u8"对方喜欢 %1"},
        {u8"我讨厌([^。！？!?\\n]{1,20})", u8"对方讨厌 %1"},
        {u8"我(?:的生日|生日)是([^。！？!?\\n]{1,15})", u8"对方的生日是 %1"},
        {u8"我最近在([^。！？!?\\n]{1,30})", u8"对方最近在 %1"},
        {u8"我想要([^。！？!?\\n]{1,30})", u8"对方想要 %1"},
        {u8"我害怕([^。！？!?\\n]{1,30})", u8"对方害怕 %1"},
        {u8"记住[:：]?([^。！？!?\\n]{1,40})", u8"%1"},
    };

    for (const auto &rule : rules) {
        const QRegularExpression re(QString::fromUtf8(rule.pattern));
        const QRegularExpressionMatch m = re.match(userText);
        if (!m.hasMatch())
            continue;

        QString fact = QString::fromUtf8(rule.fmt).arg(m.captured(1).trimmed());
        while (!fact.isEmpty() && QStringLiteral("。！？!?.，,").contains(fact.back()))
            fact.chop(1);
        if (!fact.isEmpty() && !found.contains(fact))
            found << fact;
    }
    return found;
}

#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

AppConfig &AppConfig::instance()
{
    static AppConfig cfg;
    return cfg;
}

QString AppConfig::detectRoot() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList starts = {appDir, QDir::currentPath()};
    for (const QString &start : starts) {
        QDir dir(start);
        for (int depth = 0; depth < 6; ++depth) {
            if (QFileInfo::exists(dir.filePath(QStringLiteral("database/schema.sql"))))
                return dir.absolutePath();
            if (!dir.cdUp())
                break;
        }
    }

    // Qt Creator 的 shadow build 可能位于项目旁边的兄弟目录，
    // 在上一级目录里找一次带 database/schema.sql 的项目根。
    QDir parent(appDir);
    if (parent.cdUp()) {
        const QStringList siblings = parent.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &name : siblings) {
            QDir candidate(parent.filePath(name));
            if (QFileInfo::exists(candidate.filePath(QStringLiteral("database/schema.sql"))))
                return candidate.absolutePath();
        }
    }
    return appDir;
}

void AppConfig::load()
{
    m_root = detectRoot();

    apiUrl = QStringLiteral("https://api.deepseek.com/chat/completions");
    apiKey = QStringLiteral("sk-810f77abf26a4a2fb74ac52c55a769dd");
    model = QStringLiteral("deepseek-chat");

    sqliteFile = QDir(m_root).filePath(QStringLiteral("data/xinyu.db"));

    if (QFileInfo::exists(iniPath())) {
        QSettings s(iniPath(), QSettings::IniFormat);
        apiUrl = s.value(QStringLiteral("ai/base_url"), apiUrl).toString();
        apiKey = s.value(QStringLiteral("ai/api_key"), apiKey).toString();
        model = s.value(QStringLiteral("ai/model"), model).toString();

        sqliteFile = s.value(QStringLiteral("storage/sqlite_file"), sqliteFile).toString();
        if (QFileInfo(sqliteFile).isRelative())
            sqliteFile = QDir(m_root).filePath(sqliteFile);
    }

    ensureDirs();
}

QString AppConfig::dataDir() const
{
    return QDir(m_root).filePath(QStringLiteral("data"));
}

QString AppConfig::pendingFile() const
{
    return QDir(dataDir()).filePath(QStringLiteral("pending.json"));
}

QString AppConfig::logFile() const
{
    return QDir(m_root).filePath(QStringLiteral("logs/app.log"));
}

QString AppConfig::iniPath() const
{
    return QDir(m_root).filePath(QStringLiteral("config.ini"));
}

void AppConfig::ensureDirs()
{
    QDir().mkpath(dataDir());
    QDir().mkpath(QDir(m_root).filePath(QStringLiteral("logs")));
}

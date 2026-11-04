#pragma once

#include <QString>

class AppConfig
{
public:
    static AppConfig &instance();

    void load();

    // AI 服务（仅内部使用，界面不展示）
    QString apiUrl;
    QString apiKey;
    QString model;

    // SQLite 本地存储
    QString sqliteFile;

    QString projectRoot() const { return m_root; }
    QString dataDir() const;
    QString pendingFile() const;
    QString logFile() const;
    QString iniPath() const;

private:
    AppConfig() = default;
    QString detectRoot() const;
    void ensureDirs();

    QString m_root;
};

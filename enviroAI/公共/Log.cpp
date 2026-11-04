#include "Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex g_mutex;
QFile g_file;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "INFO";
}

void handler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const QString line = QStringLiteral("%1 [%2] %3\n")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  QString::fromLatin1(levelName(type)), msg);
    QMutexLocker locker(&g_mutex);
    if (g_file.isOpen()) {
        g_file.write(line.toUtf8());
        g_file.flush();
    }
}
} // namespace

void installLogHandler(const QString &filePath)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    g_file.setFileName(filePath);
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    qInstallMessageHandler(handler);
}

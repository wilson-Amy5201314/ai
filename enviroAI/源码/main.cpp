#include "AppConfig.h"
#include "Log.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QSslSocket>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("XinYu"));
    QCoreApplication::setApplicationName(QStringLiteral("HeartIsland"));
    QApplication::setApplicationDisplayName(QStringLiteral("心屿 · 情境对话"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    const QString appDir = QCoreApplication::applicationDirPath();
    QCoreApplication::addLibraryPath(appDir);
    qputenv("PATH", (appDir + QLatin1Char(';') + qgetenv("PATH")).toUtf8());

    AppConfig::instance().load();
    installLogHandler(AppConfig::instance().logFile());

    qInfo() << "Starting 心屿" << QCoreApplication::applicationVersion()
            << "Qt" << qVersion() << "root" << AppConfig::instance().projectRoot();

    // 部分 Windows 环境下系统 TLS 通道不可用；部署时带上 OpenSSL 3，
    // 在程序内部静默切换，不向用户暴露任何连接细节。
    const QStringList tlsBackends = QSslSocket::availableBackends();
    qInfo() << "TLS backends:" << tlsBackends;
    if (tlsBackends.contains(QStringLiteral("openssl")))
        QSslSocket::setActiveBackend(QStringLiteral("openssl"));

    QFont font(Theme::fontFamily());
    font.setPointSize(10);
    app.setFont(font);
    app.setStyleSheet(Theme::qss());

    const QIcon icon(QStringLiteral(":/assets/icon.png"));
    if (!icon.isNull())
        app.setWindowIcon(icon);

    MainWindow window;
    window.show();
    return app.exec();
}

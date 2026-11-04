#pragma once

#include "Models.h"

#include <QJsonObject>
#include <QMainWindow>
#include <QVector>

namespace Ui {
class MainWindow;
}

class AiClient;
class ChatPage;
class ChatSession;
class Database;
class QDockWidget;
class QLabel;
class QProgressDialog;
class QPushButton;
class QStackedWidget;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class RolePage;
class WelcomePage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class HistoryMode { Profiles, Chat, Hidden };

    void buildUi();
    void restoreWelcomeForm();
    void onWelcomeContinue();
    void onPersonaSelected(const Persona &persona);
    void startChat(const Persona &persona, const QString &sessionId = QString(),
                   const QVector<ChatTurn> &history = {});
    void leaveChat();
    void updateDbStatus();
    void tickDatabase();
    void updateRoleCards();

    void createRole();
    void onCustomRoleReady(const QJsonObject &object);
    void onCustomRoleFailed(const QString &error);
    void saveCustomRole(const QJsonObject &object, bool fallback);

    void updateHistoryMode();
    void refreshHistory();
    void onHistoryItemClicked(QTreeWidgetItem *item, int column);
    void deleteHistorySelection();
    void deleteSelectedRole(const Persona &persona);
    void updateHistoryLockButton();
    void toggleHistoryLock();
    void activateProfile(const UserProfile &profile);
    Persona personaFromRole(const CustomRole &role, int occurrence) const;

    Ui::MainWindow *ui = nullptr;
    Database *m_db = nullptr;
    AiClient *m_chatAi = nullptr;
    AiClient *m_roleAi = nullptr;
    QStackedWidget *m_stack = nullptr;
    WelcomePage *m_welcome = nullptr;
    RolePage *m_rolePage = nullptr;
    ChatPage *m_chatPage = nullptr;
    ChatSession *m_session = nullptr;
    QTimer *m_dbTimer = nullptr;

    QDockWidget *m_historyDock = nullptr;
    QTreeWidget *m_historyTree = nullptr;
    QPushButton *m_historyDeleteButton = nullptr;
    QPushButton *m_historyLockButton = nullptr;
    QLabel *m_historyHint = nullptr;
    QLabel *m_historyLimitHint = nullptr;
    QLabel *m_historyTitle = nullptr;
    QLabel *m_historySubtitle = nullptr;
    HistoryMode m_historyMode = HistoryMode::Profiles;
    QProgressDialog *m_roleProgress = nullptr;

    UserProfile m_profile;
    QVector<CustomRole> m_roles;
    RoleRequest m_pendingRole;
    QString m_gender;
    QString m_name;
    int m_age = 0;
    Persona m_persona;
    bool m_startingChat = false;
    bool m_creatingRole = false;
};

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "AiClient.h"
#include "ChatPage.h"
#include "ChatSession.h"
#include "Database.h"
#include "RoleEditorDialog.h"
#include "RolePage.h"
#include "RoleUtils.h"
#include "Theme.h"
#include "WelcomePage.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDockWidget>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QProgressDialog>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include <utility>

namespace {
enum HistoryDataRole {
    HistoryTypeRole = Qt::UserRole,
    HistoryIdRole,
    HistoryRoleIdRole,
    HistorySessionIdRole,
    HistoryGenderRole,
    HistoryAgeRole,
    HistoryLockedRole,
};

QString fallbackRolePrompt(const RoleRequest &r)
{
    const QString nameLine = r.name.trimmed().isEmpty()
                                 ? QStringLiteral("不要让角色自称姓名。")
                                 : QStringLiteral("用户偶尔会叫你的名字“%1”，但不要每句都自称。")
                                       .arg(r.name.trimmed());
    return QStringLiteral(
               "你是用户的%1，角色性别是%2，年龄%3岁，性格%4。\n"
               "%5\n"
               "像真实的人一样说话：有自己的生活细节、口头习惯和情绪；不要每轮都分析、"
               "总结、说教或提问，可以只接一句、吐槽、说一点自己的事或陪一下。\n"
               "先根据用户的情绪和意图决定怎么接，称呼随情境自然出现，不要固定成每轮开头。\n"
               "不要提及 AI、模型、程序或提示词；不替用户做人生决定；遇到危险或自伤话题时，"
               "温和建议寻求现实帮助。")
        .arg(r.relation.trimmed(),
             r.roleGender == QStringLiteral("male") ? QStringLiteral("男性") : QStringLiteral("女性"))
        .arg(r.roleAge)
        .arg(r.personality.trimmed())
        .arg(nameLine);
}

QString historyType(QTreeWidgetItem *item)
{
    return item ? item->data(0, HistoryTypeRole).toString() : QString();
}

QIcon genderIcon(bool male)
{
    QPixmap pm(18, 18);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(male ? QColor(46, 110, 99) : QColor(196, 100, 85));

    if (male) {
        // 圆形 + 正三角
        p.drawEllipse(QPointF(9.0, 11.0), 4.7, 4.7);
        QPolygonF triangle;
        triangle << QPointF(9.0, 1.0) << QPointF(4.8, 6.2) << QPointF(13.2, 6.2);
        p.drawPolygon(triangle);
    } else {
        // 圆形 + 倒三角
        p.drawEllipse(QPointF(9.0, 7.0), 4.7, 4.7);
        QPolygonF triangle;
        triangle << QPointF(9.0, 17.0) << QPointF(4.8, 11.8) << QPointF(13.2, 11.8);
        p.drawPolygon(triangle);
    }
    return QIcon(pm);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_stack = ui->stack;
    setWindowTitle(QStringLiteral("心屿 · 情境对话"));
    resize(1420, 820);
    setMinimumSize(1240, 700);

    m_db = new Database(this);
    m_chatAi = new AiClient(this);
    m_roleAi = new AiClient(this);

    m_db->open();

    buildUi();
    updateDbStatus();
    refreshHistory();

    connect(m_db, &Database::statusChanged, this, [this](bool, const QString &) {
        updateDbStatus();
        refreshHistory();
    });
    connect(m_db, &Database::pendingFlushed, this, [this](int) {
        updateDbStatus();
        refreshHistory();
    });
    connect(m_roleAi, &AiClient::customRoleReady, this, &MainWindow::onCustomRoleReady);
    connect(m_roleAi, &AiClient::customRoleFailed, this, &MainWindow::onCustomRoleFailed);

    m_dbTimer = new QTimer(this);
    m_dbTimer->setInterval(3000);
    connect(m_dbTimer, &QTimer::timeout, this, &MainWindow::tickDatabase);
    m_dbTimer->start();
}

MainWindow::~MainWindow()
{
    if (m_session)
        m_session->abort();
    if (m_chatAi)
        m_chatAi->abort();
    if (m_roleAi)
        m_roleAi->abort();
    if (m_db)
        m_db->close();
    delete ui;
}

void MainWindow::buildUi()
{
    m_welcome = new WelcomePage(m_stack);
    m_rolePage = new RolePage(m_stack);
    m_chatPage = new ChatPage(m_stack);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_rolePage);
    m_stack->addWidget(m_chatPage);

    connect(m_welcome, &WelcomePage::genderSelected, this, [this](const QString &gender) {
        m_gender = gender;
    });
    connect(m_welcome, &WelcomePage::continueRequested, this, &MainWindow::onWelcomeContinue);

    connect(m_rolePage, &RolePage::backRequested, this, [this] {
        // 返回用户角色页面时清空上一次填写的姓名，避免带进下一次创建。
        m_name.clear();
        restoreWelcomeForm();
        m_stack->setCurrentWidget(m_welcome);
    });
    connect(m_rolePage, &RolePage::createRoleRequested, this, &MainWindow::createRole);
    connect(m_rolePage, &RolePage::deleteRoleRequested, this, &MainWindow::deleteSelectedRole);
    connect(m_rolePage, &RolePage::roleNameSubmitted, this,
            [this](const QString &personaKey, const QString &name) {
                if (!m_profile.isValid() || name.trimmed().isEmpty())
                    return;
                // 延迟刷新，避免在角色卡发出信号的过程中直接重建并销毁它。
                QTimer::singleShot(0, this, [this, personaKey, name] {
                    if (!m_profile.isValid())
                        return;
                    if (!m_db->setRoleName(personaKey, name)) {
                        QMessageBox::information(this, QStringLiteral("名字已填写"),
                                                 QStringLiteral("这个名字已经填写过，不能再次修改。"));
                        return;
                    }
                    m_roles = m_db->rolesForProfile(m_profile.id, 5);
                    updateRoleCards();
                    m_rolePage->selectPersona(personaKey);
                    refreshHistory();
                });
            });
    connect(m_rolePage, &RolePage::personaSelected, this, &MainWindow::onPersonaSelected);

    connect(m_chatPage, &ChatPage::backRequested, this, &MainWindow::leaveChat);
    connect(m_chatPage, &ChatPage::sendRequested, this, [this](const QString &text) {
        if (m_session)
            m_session->send(text);
    });
    connect(m_chatPage, &ChatPage::restartRequested, this, [this] {
        if (!m_session || !m_persona.isValid())
            return;
        m_startingChat = false;
        startChat(m_persona);
    });

    m_historyDock = new QDockWidget(this);
    m_historyDock->setObjectName(QStringLiteral("historyDock"));
    m_historyDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_historyDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    m_historyDock->setMinimumWidth(225);
    m_historyDock->setMaximumWidth(270);

    auto *historyHost = new QWidget(m_historyDock);
    historyHost->setObjectName(QStringLiteral("historyHost"));
    auto *historyLayout = new QVBoxLayout(historyHost);
    historyLayout->setContentsMargins(14, 16, 12, 12);
    historyLayout->setSpacing(7);
    m_historyTitle = new QLabel(QStringLiteral("以往记录"), historyHost);
    m_historyTitle->setObjectName(QStringLiteral("historyTitle"));
    m_historySubtitle = new QLabel(historyHost);
    m_historySubtitle->setObjectName(QStringLiteral("historySubtitle"));
    m_historySubtitle->setWordWrap(true);
    m_historyTree = new QTreeWidget(historyHost);
    m_historyTree->setObjectName(QStringLiteral("historyTree"));
    m_historyTree->setHeaderHidden(true);
    m_historyTree->setIndentation(14);
    m_historyTree->setUniformRowHeights(true);
    m_historyTree->setRootIsDecorated(true);
    m_historyTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_historyTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_historyTree->setIconSize(QSize(18, 18));
    m_historyDeleteButton = new QPushButton(QStringLiteral("删除选中记录"), historyHost);
    m_historyDeleteButton->setObjectName(QStringLiteral("ghost"));
    m_historyDeleteButton->setEnabled(false);
    m_historyDeleteButton->setMinimumHeight(34);
    m_historyHint = new QLabel(historyHost);
    m_historyHint->setObjectName(QStringLiteral("historyHint"));
    m_historyHint->setWordWrap(true);
    m_historyHint->setVisible(false);
    m_historyLimitHint = new QLabel(historyHost);
    m_historyLimitHint->setObjectName(QStringLiteral("historyHint"));
    m_historyLimitHint->setWordWrap(true);
    m_historyLimitHint->setVisible(false);
    m_historyLockButton = new QPushButton(QStringLiteral("☆ 锁定选中"), historyHost);
    m_historyLockButton->setObjectName(QStringLiteral("ghost"));
    m_historyLockButton->setEnabled(false);
    m_historyLockButton->setMinimumHeight(34);
    m_historyLockButton->setVisible(false);
    historyLayout->addWidget(m_historyTitle);
    historyLayout->addWidget(m_historySubtitle);
    historyLayout->addWidget(m_historyTree, 1);
    historyLayout->addWidget(m_historyHint);
    historyLayout->addWidget(m_historyLimitHint);
    historyLayout->addWidget(m_historyLockButton);
    historyLayout->addWidget(m_historyDeleteButton);
    m_historyDock->setWidget(historyHost);
    m_historyDock->setTitleBarWidget(new QWidget(m_historyDock));
    addDockWidget(Qt::LeftDockWidgetArea, m_historyDock);
    resizeDocks({m_historyDock}, {245}, Qt::Horizontal);
    connect(m_historyTree, &QTreeWidget::itemClicked, this, &MainWindow::onHistoryItemClicked);
    connect(m_historyTree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        // 右键只选中，不弹删除菜单；删除统一由下方按钮完成。
        if (QTreeWidgetItem *item = m_historyTree->itemAt(pos))
            m_historyTree->setCurrentItem(item);
    });
    connect(m_historyTree, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto selected = m_historyTree->selectedItems();
        m_historyDeleteButton->setEnabled(!selected.isEmpty());
        updateHistoryLockButton();
    });
    connect(m_historyDeleteButton, &QPushButton::clicked, this, &MainWindow::deleteHistorySelection);
    connect(m_historyLockButton, &QPushButton::clicked, this, &MainWindow::toggleHistoryLock);
    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int) { updateHistoryMode(); });
    updateHistoryMode();
    restoreWelcomeForm();
}

void MainWindow::restoreWelcomeForm()
{
    if (!m_welcome)
        return;
    if (!m_gender.isEmpty())
        m_welcome->selectGender(m_gender);
    if (m_age > 0)
        m_welcome->selectAge(m_age);
    m_welcome->selectName(m_name);
    m_welcome->setContinueEnabled(!m_gender.isEmpty());
    m_welcome->focusNameInput();
}

void MainWindow::updateHistoryMode()
{
    if (!m_historyDock)
        return;

    if (m_stack->currentWidget() == m_welcome) {
        m_historyMode = HistoryMode::Profiles;
        m_historyTitle->setText(QStringLiteral("你的资料"));
        m_historySubtitle->setText(QStringLiteral("选择一份资料，进入你的角色列表"));
        m_historyHint->setText(QStringLiteral("左键进入，右键选中"));
        m_historyHint->setVisible(true);
        m_historyLimitHint->setVisible(false);
        m_historyLockButton->setVisible(false);
    } else if (m_stack->currentWidget() == m_chatPage) {
        m_historyMode = HistoryMode::Chat;
        m_historyTitle->setText(QStringLiteral("这段关系的对话"));
        m_historySubtitle->setText(m_persona.displayName.isEmpty()
                                       ? QStringLiteral("只显示当前角色的以往对话")
                                       : QStringLiteral("%1 的以往对话").arg(m_persona.displayName));
        m_historyHint->setText(QStringLiteral("左键打开，右键选中"));
        m_historyHint->setVisible(true);
        m_historyLimitHint->setText(
            QStringLiteral("最多显示 10 条；超过后，未锁定的最早记录会被替换。"));
        m_historyLimitHint->setVisible(true);
        m_historyLockButton->setVisible(true);
    } else {
        m_historyMode = HistoryMode::Hidden;
        m_historyDock->setVisible(false);
        return;    }

    m_historyDock->setVisible(true);
    refreshHistory();
}

void MainWindow::onWelcomeContinue()
{
    m_gender = m_welcome->selectedGender();
    m_age = m_welcome->selectedAge();
    m_name = m_welcome->selectedName();
    if (m_gender.isEmpty())
        return;

    QString error;
    m_profile = m_db->ensureProfile(m_gender, m_age, m_name, &error);
    if (!m_profile.isValid()) {
        if (error == QStringLiteral("user profile limit reached")) {
            m_welcome->setProfileLimitReached(true);
            QMessageBox::information(this, QStringLiteral("用户角色已满"),
                                     QStringLiteral("最多保存 5 个用户角色（性别 + 年龄）。\n请在左侧删除一个用户角色，然后再创建。"));
        } else {
            QMessageBox::information(this, QStringLiteral("还没准备好"),
                                     error.isEmpty() ? QStringLiteral("请稍后再试。") : error);
        }
        return;
    }

    m_welcome->setContinueEnabled(false);
    updateRoleCards();
    refreshHistory();
    m_stack->setCurrentWidget(m_rolePage);
}

void MainWindow::updateRoleCards()
{
    if (!m_profile.isValid()) {
        m_roles.clear();
        m_rolePage->setPersonas({}, m_gender);
        return;
    }

    m_roles = m_db->rolesForProfile(m_profile.id, 5);
    QHash<QString, int> relationCount;
    QVector<Persona> personas;
    for (const CustomRole &role : std::as_const(m_roles)) {
        const int occurrence = ++relationCount[role.relation.trimmed()];
        personas.append(personaFromRole(role, occurrence));
    }
    m_gender = m_profile.gender;
    m_rolePage->setPersonas(personas, m_gender);
    m_rolePage->setProfileAge(m_profile.age, m_profile.name);
    m_rolePage->setCreateEnabled(m_roles.size() < 5);
}

Persona MainWindow::personaFromRole(const CustomRole &role, int occurrence) const
{
    Persona p;
    p.key = role.id;
    p.roleKey = roleStyleKey(role.relation, role.roleGender);
    p.roleName = role.relation;
    p.relation = role.relation;
    p.name = role.name;
    p.roleGender = role.roleGender;
    p.roleAge = role.age;
    p.personality = role.personality;
    p.displayName = roleDisplayLabel(role.relation, role.name, occurrence);
    p.avatar = role.avatar.trimmed();
    if (p.avatar.isEmpty() || p.avatar.size() > 8)
        p.avatar = Theme::roleEmoji(p.roleKey);
    p.tagline = role.tagline;
    p.systemPrompt = role.systemPrompt;
    return p;
}

void MainWindow::onPersonaSelected(const Persona &persona)
{
    if (m_startingChat)
        return;
    m_rolePage->setLaunching(true);
    m_persona = persona;
    startChat(persona);
}

void MainWindow::createRole()
{
    if (!m_profile.isValid() || m_gender.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("先选身份"),
                                 QStringLiteral("请先回到开始页选择性别和年龄。"));
        return;
    }
    if (m_roles.size() >= 5) {
        QMessageBox::information(this, QStringLiteral("角色已满"),
                                 QStringLiteral("每个用户最多创建 5 个角色。"));
        return;
    }
    if (m_creatingRole || m_roleAi->busy())
        return;

    RoleEditorDialog dialog(m_profile, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_pendingRole = dialog.request();
    if (!m_pendingRole.isValid())
        return;

    m_creatingRole = true;
    m_roleProgress = new QProgressDialog(
        QStringLiteral("正在为「%1」生成角色…").arg(m_pendingRole.relation), QString(), 0, 0, this);
    m_roleProgress->setWindowTitle(QStringLiteral("创建角色"));
    m_roleProgress->setWindowModality(Qt::WindowModal);
    m_roleProgress->setCancelButton(nullptr);
    m_roleProgress->setMinimumDuration(0);
    m_roleProgress->setAutoClose(false);
    m_roleProgress->show();

    m_roleAi->generateCustomRole(m_pendingRole);
}

void MainWindow::onCustomRoleReady(const QJsonObject &object)
{
    if (!m_creatingRole)
        return;
    saveCustomRole(object, false);
}

void MainWindow::onCustomRoleFailed(const QString &error)
{
    if (!m_creatingRole)
        return;
    qWarning() << "Custom role generation failed:" << error;
    saveCustomRole({}, true);
}

void MainWindow::saveCustomRole(const QJsonObject &object, bool fallback)
{
    if (!m_creatingRole)
        return;

    CustomRole role;
    role.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    role.profileId = m_profile.id;
    role.relation = m_pendingRole.relation.trimmed();
    role.name = m_pendingRole.name.trimmed();
    role.roleGender = m_pendingRole.roleGender;
    role.age = m_pendingRole.roleAge;
    role.personality = m_pendingRole.personality.trimmed();
    role.systemPrompt = object.value(QStringLiteral("system_prompt")).toString().trimmed();
    if (role.systemPrompt.isEmpty())
        role.systemPrompt = fallbackRolePrompt(m_pendingRole);
    role.tagline = object.value(QStringLiteral("tagline")).toString().trimmed();
    if (role.tagline.isEmpty()) {
        role.tagline = QStringLiteral("%1岁 · %2")
                           .arg(role.age)
                           .arg(role.personality);
    }
    role.avatar = object.value(QStringLiteral("avatar")).toString().trimmed();
    if (role.avatar.isEmpty() || role.avatar.size() > 8)
        role.avatar = QStringLiteral("✨");
    role.createdAt = QDateTime::currentDateTime();

    const bool saved = m_db->insertRole(role);

    if (m_roleProgress) {
        m_roleProgress->close();
        m_roleProgress->deleteLater();
        m_roleProgress = nullptr;
    }
    m_creatingRole = false;
    m_pendingRole = {};

    if (!saved) {
        QMessageBox::warning(this, QStringLiteral("创建失败"),
                             QStringLiteral("角色没能保存到本地数据库，请重试。"));
        return;
    }

    updateRoleCards();
    refreshHistory();
    m_rolePage->selectPersona(role.id);
    if (fallback) {
        QMessageBox::information(this, QStringLiteral("先用基础设定"),
                                 QStringLiteral("AI 暂时没接上，已用你填写的信息创建角色；稍后重新创建一个会更丰富。"));
    }
}

void MainWindow::startChat(const Persona &persona, const QString &sessionId,
                           const QVector<ChatTurn> &history)
{
    if (!persona.isValid() || m_gender.isEmpty() || m_startingChat)
        return;

    m_startingChat = true;
    QTimer::singleShot(450, this, [this] { m_startingChat = false; });

    if (m_session) {
        m_chatAi->abort();
        m_session->abort();
        delete m_session;
        m_session = nullptr;
    }

    m_persona = persona;
    m_rolePage->selectPersona(persona.key);

    m_session = new ChatSession(persona, m_gender, m_profile.name, m_db, m_chatAi, this);
    connect(m_session, &ChatSession::messageAdded, m_chatPage, &ChatPage::addBubble);
    connect(m_session, &ChatSession::assistantStarted, m_chatPage, &ChatPage::beginAssistantStream);
    connect(m_session, &ChatSession::assistantDelta, m_chatPage, &ChatPage::appendAssistantDelta);
    connect(m_session, &ChatSession::assistantFinished, m_chatPage, &ChatPage::finishAssistantStream);
    connect(m_session, &ChatSession::assistantFailed, m_chatPage, &ChatPage::failAssistant);
    connect(m_session, &ChatSession::busyChanged, m_chatPage, &ChatPage::setBusy);
    connect(m_session, &ChatSession::messageAdded, this, [this] { refreshHistory(); });
    connect(m_session, &ChatSession::assistantFinished, this, [this] { refreshHistory(); });

    m_chatPage->begin(persona);
    if (!sessionId.isEmpty()) {
        m_session->restore(sessionId, history);
        for (const ChatTurn &turn : history)
            m_chatPage->addBubble(turn);
    } else {
        m_session->start();
    }
    m_chatPage->setMemoryStatus(m_db->isOnline(), m_db->pendingCount());
    m_stack->setCurrentWidget(m_chatPage);
    updateDbStatus();
    refreshHistory();
}

void MainWindow::leaveChat()
{
    if (m_session) {
        m_chatAi->abort();
        m_session->abort();
        delete m_session;
        m_session = nullptr;
    }
    m_chatPage->clearConversation();
    m_chatPage->setBusy(false);
    m_rolePage->setLaunching(false);
    if (m_persona.isValid())
        m_rolePage->selectPersona(m_persona.key);
    m_stack->setCurrentWidget(m_rolePage);
    updateDbStatus();
    refreshHistory();
}

void MainWindow::updateDbStatus()
{
    if (!m_db || !m_welcome || !m_chatPage)
        return;
    const bool online = m_db->isOnline();
    const int pending = m_db->pendingCount();
    m_welcome->setMemoryStatus(online, pending);
    m_chatPage->setMemoryStatus(online, pending);
}

void MainWindow::tickDatabase()
{
    if (!m_db)
        return;
    if (!m_db->isOnline()) {
        m_db->open();
        refreshHistory();
    }
}

void MainWindow::refreshHistory()
{
    if (!m_historyTree)
        return;

    QSignalBlocker blocker(m_historyTree);
    m_historyTree->clear();
    if (m_historyDeleteButton)
        m_historyDeleteButton->setEnabled(false);
    if (m_historyLockButton)
        m_historyLockButton->setEnabled(false);

    if (m_historyMode == HistoryMode::Hidden)
        return;

    if (!m_db->isOnline()) {
        auto *offline = new QTreeWidgetItem(m_historyTree);
        offline->setText(0, QStringLiteral("正在连接本地记录…"));
        offline->setFlags(Qt::NoItemFlags);
        return;
    }

    if (m_historyMode == HistoryMode::Profiles) {
        const QVector<UserProfile> profiles = m_db->profiles(5);
        m_welcome->setProfileLimitReached(profiles.size() >= 5);
        m_historySubtitle->setText(profiles.isEmpty()
                                       ? QStringLiteral("最多 5 个")
                                       : QStringLiteral("共 %1/5 · 最多 5 个")
                                             .arg(profiles.size()));
        if (profiles.isEmpty()) {
            auto *empty = new QTreeWidgetItem(m_historyTree);
            empty->setText(0, QStringLiteral("还没有资料，先在右侧选择性别和年龄"));
            empty->setFlags(Qt::NoItemFlags);
            return;
        }
        for (const UserProfile &profile : profiles) {
            auto *profileItem = new QTreeWidgetItem(m_historyTree);
            const bool isMale = profile.gender == QStringLiteral("male");
            profileItem->setIcon(0, genderIcon(isMale));
            profileItem->setText(0, profile.listLabel());
            profileItem->setToolTip(0, profile.name.trimmed().isEmpty()
                                           ? QStringLiteral("%1 · 未填写姓名")
                                                 .arg(isMale ? QStringLiteral("男生")
                                                             : QStringLiteral("女生"))
                                           : QStringLiteral("%1 · 姓名：%2")
                                                 .arg(isMale ? QStringLiteral("男生")
                                                             : QStringLiteral("女生"))
                                                 .arg(profile.name));
            profileItem->setSizeHint(0, QSize(0, 38));
            profileItem->setData(0, HistoryTypeRole, QStringLiteral("profile"));
            profileItem->setData(0, HistoryIdRole, profile.id);
            profileItem->setData(0, HistoryGenderRole, profile.gender);
            profileItem->setData(0, HistoryAgeRole, profile.age);
            if (profile.id == m_profile.id) {
                QFont font = profileItem->font(0);
                font.setBold(true);
                profileItem->setFont(0, font);
            }
        }
        return;
    }

    if (m_historyMode == HistoryMode::Chat) {
        if (!m_persona.isValid()) {
            auto *empty = new QTreeWidgetItem(m_historyTree);
            empty->setText(0, QStringLiteral("还没有进入对话"));
            empty->setFlags(Qt::NoItemFlags);
            return;
        }
        const QVector<SessionInfo> sessions = m_db->sessionsForRole(m_persona.key, 10);
        m_historySubtitle->setText(QStringLiteral("%1 的以往对话 · 共 %2 次")
                                       .arg(m_persona.displayName)
                                       .arg(sessions.size()));
        if (sessions.isEmpty()) {
            auto *empty = new QTreeWidgetItem(m_historyTree);
            empty->setText(0, QStringLiteral("这段关系还没有对话"));
            empty->setFlags(Qt::NoItemFlags);
            return;
        }
        for (const SessionInfo &session : sessions) {
            auto *sessionItem = new QTreeWidgetItem(m_historyTree);
            const QString prefix = session.locked ? QStringLiteral("★  ") : QStringLiteral("💬  ");
            sessionItem->setText(0, prefix + (session.createdAt.isValid()
                                                  ? session.createdAt.toString(QStringLiteral("MM-dd HH:mm"))
                                                  : QStringLiteral("一次对话")));
            sessionItem->setSizeHint(0, QSize(0, 36));
            sessionItem->setData(0, HistoryTypeRole, QStringLiteral("session"));
            sessionItem->setData(0, HistoryIdRole, m_persona.key);
            sessionItem->setData(0, HistorySessionIdRole, session.id);
            sessionItem->setData(0, HistoryRoleIdRole, m_profile.id);
            sessionItem->setData(0, HistoryGenderRole, m_profile.gender);
            sessionItem->setData(0, HistoryAgeRole, m_profile.age);
            sessionItem->setData(0, HistoryLockedRole, session.locked);
        }
        updateHistoryLockButton();
    }
}

void MainWindow::onHistoryItemClicked(QTreeWidgetItem *item, int)
{
    if (!item)
        return;

    const QString type = historyType(item);
    if (type == QStringLiteral("profile")) {
        const UserProfile profile = m_db->profileById(item->data(0, HistoryIdRole).toString());
        if (profile.isValid())
            activateProfile(profile);
        return;
    }

    if (type == QStringLiteral("role")) {
        const CustomRole role = m_db->roleById(item->data(0, HistoryIdRole).toString());
        if (!role.isValid())
            return;
        const UserProfile profile = m_db->profileById(role.profileId);
        if (profile.isValid())
            activateProfile(profile);
        m_rolePage->selectPersona(role.id);
        m_stack->setCurrentWidget(m_rolePage);
        return;
    }

    if (type == QStringLiteral("session")) {
        const QString roleId = item->data(0, HistoryIdRole).toString();
        const QString sessionId = item->data(0, HistorySessionIdRole).toString();
        const CustomRole role = m_db->roleById(roleId);
        if (!role.isValid() || sessionId.isEmpty())
            return;
        const UserProfile profile = m_db->profileById(role.profileId);
        if (!profile.isValid())
            return;
        activateProfile(profile);

        int occurrence = 1;
        int count = 0;
        const QVector<CustomRole> roles = m_db->rolesForProfile(profile.id, 5);
        for (const CustomRole &candidate : roles) {
            if (candidate.relation.trimmed() == role.relation.trimmed())
                ++count;
            if (candidate.id == role.id) {
                occurrence = count;
                break;
            }
        }
        const Persona persona = personaFromRole(role, occurrence);
        const QVector<ChatTurn> history = m_db->messagesForSession(sessionId);
        startChat(persona, sessionId, history);
    }
}

void MainWindow::updateHistoryLockButton()
{
    if (!m_historyLockButton)
        return;
    bool canLock = false;
    bool locked = false;
    if (m_historyMode == HistoryMode::Chat) {
        const auto selected = m_historyTree->selectedItems();
        if (selected.size() == 1 && historyType(selected.first()) == QStringLiteral("session")) {
            canLock = true;
            locked = selected.first()->data(0, HistoryLockedRole).toBool();
        }
    }
    m_historyLockButton->setEnabled(canLock);
    m_historyLockButton->setText(locked ? QStringLiteral("★ 取消锁定")
                                        : QStringLiteral("☆ 锁定选中"));
}

void MainWindow::toggleHistoryLock()
{
    if (m_historyMode != HistoryMode::Chat)
        return;
    const auto selected = m_historyTree->selectedItems();
    if (selected.size() != 1)
        return;
    QTreeWidgetItem *item = selected.first();
    if (historyType(item) != QStringLiteral("session"))
        return;
    const QString sessionId = item->data(0, HistorySessionIdRole).toString();
    if (sessionId.isEmpty())
        return;
    const bool locked = item->data(0, HistoryLockedRole).toBool();
    if (m_db->setSessionLocked(sessionId, !locked))
        refreshHistory();
}

void MainWindow::deleteHistorySelection()
{
    if (!m_historyTree)
        return;
    const auto selected = m_historyTree->selectedItems();
    if (selected.isEmpty())
        return;
    QTreeWidgetItem *item = selected.first();
    const QString type = historyType(item);

    if (type == QStringLiteral("profile")) {
        const QString profileId = item->data(0, HistoryIdRole).toString();
        if (profileId.isEmpty())
            return;
        if (QMessageBox::question(this, QStringLiteral("删除用户角色"),
                                  QStringLiteral("删除这个用户角色及其全部关系角色、聊天记录吗？"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
            return;
        if (!m_db->deleteProfile(profileId))
            return;
        if (m_profile.id == profileId) {
            m_profile = {};
            m_roles.clear();
            m_rolePage->setPersonas({}, m_gender);
        }
        refreshHistory();
        return;
    }

    if (type == QStringLiteral("session")) {
        const QString sessionId = item->data(0, HistorySessionIdRole).toString();
        const QString roleId = item->data(0, HistoryIdRole).toString();
        if (sessionId.isEmpty() || roleId.isEmpty())
            return;
        if (QMessageBox::question(this, QStringLiteral("删除聊天记录"),
                                  QStringLiteral("删除这次对话及其全部消息吗？"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
            return;
        const bool current = m_session && m_session->id() == sessionId;
        if (current)
            leaveChat();
        m_db->deleteSession(sessionId);
        refreshHistory();
    }
}

void MainWindow::deleteSelectedRole(const Persona &persona)
{
    if (!persona.isValid())
        return;
    if (QMessageBox::question(this, QStringLiteral("删除关系角色"),
                              QStringLiteral("删除「%1」及其全部聊天记录吗？").arg(persona.displayName),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;
    if (!m_db->deleteRole(persona.key))
        return;
    updateRoleCards();
    refreshHistory();
}


void MainWindow::activateProfile(const UserProfile &profile)
{
    m_profile = profile;
    m_gender = profile.gender;
    m_name = profile.name;
    m_age = profile.age;
    m_welcome->selectGender(profile.gender);
    m_welcome->selectAge(profile.age);
    m_welcome->selectName(profile.name);
    m_welcome->setContinueEnabled(true);
    updateRoleCards();
    refreshHistory();
    m_stack->setCurrentWidget(m_rolePage);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_session)
        m_session->abort();
    if (m_chatAi)
        m_chatAi->abort();
    if (m_roleAi)
        m_roleAi->abort();
    if (m_db)
        m_db->close();
    QMainWindow::closeEvent(event);
}

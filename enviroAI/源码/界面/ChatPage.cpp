#include "ChatPage.h"
#include "ui_ChatPage.h"

#include "AvatarWidget.h"
#include "ChatBubble.h"
#include "Theme.h"
#include "TypingIndicator.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    ui->messageHost->setAttribute(Qt::WA_TranslucentBackground);
    ui->input->installEventFilter(this);
    ui->composer->setFixedHeight(72);
    ui->composerWrap->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->outerLayout->setStretch(2, 1);
    ui->messageLayout->setStretch(0, 1);
    ui->messageLayout->setStretch(1, 0);

    m_streamTimer = new QTimer(this);
    m_streamTimer->setSingleShot(true);
    m_streamTimer->setInterval(50);
    connect(m_streamTimer, &QTimer::timeout, this, [this] {
        if (m_streamingBubble)
            m_streamingBubble->setText(m_streamingText);
        scrollToBottom();
    });

    connect(ui->backButton, &QPushButton::clicked, this, &ChatPage::backRequested);
    connect(ui->sendButton, &QPushButton::clicked, this, &ChatPage::sendCurrent);
    connect(ui->restartButton, &QPushButton::clicked, this, &ChatPage::restartRequested);
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::begin(const Persona &persona)
{
    m_persona = persona;

    const QColor accent = Theme::roleAccent(persona.roleKey);
    ui->avatar->setEmoji(persona.avatar.isEmpty() ? Theme::roleEmoji(persona.roleKey) : persona.avatar);
    ui->avatar->setAccent(accent);
    ui->nameLabel->setText(persona.displayName.isEmpty() ? persona.roleName : persona.displayName);
    ui->roleChip->setVisible(false);
    ui->subtitleLabel->setText(QStringLiteral("正在听你说话"));
    ui->subtitleLabel->setToolTip(QString());

    clearConversation();
    setBusy(false);
    ui->input->clear();
    ui->input->setFocus();
}

void ChatPage::clearConversation()
{
    clearMessageWidgets();
    m_streamingBubble = nullptr;
    m_streamingText.clear();
    m_typing = nullptr;
    if (ui->emptyState)
        showEmptyState(true);
}

void ChatPage::clearMessageWidgets()
{
    if (!ui || !ui->messageLayout)
        return;
    if (m_streamTimer)
        m_streamTimer->stop();
    // 保留索引 0 的空状态和末尾的 stretch，中间是消息、输入动画等。
    for (int i = ui->messageLayout->count() - 2; i >= 1; --i) {
        QLayoutItem *item = ui->messageLayout->takeAt(i);
        if (QWidget *w = item->widget())
            delete w;
        delete item;
    }
    m_typing = nullptr;
    m_streamingBubble = nullptr;
    m_streamingText.clear();
    showEmptyState(true);
}

void ChatPage::addBubble(const ChatTurn &turn)
{
    if (turn.role == QStringLiteral("system")) {
        addNotice(turn.content);
        return;
    }
    if (ui->emptyState)
        showEmptyState(false);
    auto *bubble = new ChatBubble(turn.role, turn.content,
                                  m_persona.avatar.isEmpty() ? Theme::roleEmoji(m_persona.roleKey)
                                                             : m_persona.avatar,
                                  Theme::roleAccent(m_persona.roleKey), ui->messageHost);
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, bubble);
    scrollToBottom();
}

void ChatPage::beginAssistantStream()
{
    showTyping(false);
    if (m_streamTimer)
        m_streamTimer->stop();
    if (m_streamingBubble)
        return;
    if (ui->emptyState)
        showEmptyState(false);
    m_streamingText.clear();
    m_streamingBubble = new ChatBubble(QStringLiteral("assistant"), QString(),
                                       m_persona.avatar.isEmpty() ? Theme::roleEmoji(m_persona.roleKey)
                                                                  : m_persona.avatar,
                                       Theme::roleAccent(m_persona.roleKey), ui->messageHost);
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, m_streamingBubble);
    scrollToBottom();
}

void ChatPage::appendAssistantDelta(const QString &delta)
{
    showTyping(false);
    if (!m_streamingBubble)
        beginAssistantStream();
    m_streamingText += delta;
    // 流式 token 到达频率可能很高；这里按 50ms 合并刷新，避免每字重排整个气泡。
    if (m_streamTimer && !m_streamTimer->isActive())
        m_streamTimer->start();
}

void ChatPage::finishAssistantStream(const QString &fullText)
{
    if (m_streamTimer)
        m_streamTimer->stop();
    showTyping(false);
    if (m_streamingBubble) {
        m_streamingBubble->setText(fullText);
        m_streamingBubble = nullptr;
    } else {
        ChatTurn turn;
        turn.role = QStringLiteral("assistant");
        turn.content = fullText;
        turn.at = QDateTime::currentDateTime();
        addBubble(turn);
    }
    m_streamingText.clear();
    scrollToBottom();
}

void ChatPage::failAssistant(const QString &error)
{
    if (m_streamTimer)
        m_streamTimer->stop();
    showTyping(false);
    if (m_streamingBubble) {
        m_streamingBubble->deleteLater();
        m_streamingBubble = nullptr;
    }
    m_streamingText.clear();
    addNotice(error.isEmpty() ? QStringLiteral("刚才没能接上话，再试一次吧。") : error);
    ui->input->setEnabled(true);
    ui->sendButton->setEnabled(true);
    ui->typingHint->setVisible(false);
    ui->input->setFocus();
}

void ChatPage::addNotice(const QString &text)
{
    if (ui->emptyState)
        showEmptyState(false);
    auto *notice = new QLabel(text, ui->messageHost);
    notice->setObjectName(QStringLiteral("notice"));
    notice->setWordWrap(true);
    notice->setAlignment(Qt::AlignCenter);
    notice->setMaximumWidth(520);
    auto *row = new QWidget(ui->messageHost);
    row->setAttribute(Qt::WA_TranslucentBackground);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->addStretch(1);
    layout->addWidget(notice);
    layout->addStretch(1);
    ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, row);
    scrollToBottom();
}

void ChatPage::setBusy(bool busy)
{
    m_busy = busy;
    ui->input->setEnabled(!busy);
    ui->sendButton->setEnabled(!busy);
    ui->typingHint->setVisible(busy);
    ui->input->setPlaceholderText(busy ? QStringLiteral("正在回应…")
                                       : QStringLiteral("说点什么…  Enter 发送"));
    if (busy) {
        if (!m_streamingBubble)
            showTyping(true);
    } else {
        showTyping(false);
        ui->input->setFocus();
    }
}

void ChatPage::showTyping(bool show)
{
    if (show) {
        if (m_typing)
            return;
        m_typing = new TypingIndicator(Theme::roleAccent(m_persona.roleKey), ui->messageHost);
        ui->messageLayout->insertWidget(ui->messageLayout->count() - 1, m_typing);
        scrollToBottom();
    } else if (m_typing) {
        ui->messageLayout->removeWidget(m_typing);
        m_typing->deleteLater();
        m_typing = nullptr;
    }
}

void ChatPage::showEmptyState(bool visible)
{
    if (!ui || !ui->emptyState || !ui->messageLayout)
        return;
    ui->emptyState->setVisible(visible);
    ui->emptyState->setSizePolicy(QSizePolicy::Preferred,
                                  visible ? QSizePolicy::Expanding : QSizePolicy::Preferred);
    ui->messageLayout->setStretch(0, visible ? 1 : 0);
    const int last = ui->messageLayout->count() - 1;
    if (last > 0)
        ui->messageLayout->setStretch(last, visible ? 0 : 1);
}

void ChatPage::sendCurrent()
{
    if (m_busy)
        return;
    const QString text = ui->input->toPlainText().trimmed();
    if (text.isEmpty())
        return;
    ui->input->clear();
    emit sendRequested(text);
}

void ChatPage::scrollToBottom()
{
    QTimer::singleShot(0, this, [this] {
        if (!ui || !ui->scrollArea)
            return;
        auto *bar = ui->scrollArea->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
}

void ChatPage::setMemoryStatus(bool online, int pending)
{
    if (online) {
        ui->memoryLabel->setObjectName(QStringLiteral("memoryOnline"));
        ui->memoryLabel->setText(QStringLiteral("●  记忆已连接"));
        ui->memoryLabel->setToolTip(QStringLiteral("角色与对话记录会保存在本地数据库中。"));
    } else {
        ui->memoryLabel->setObjectName(QStringLiteral("memoryOffline"));
        ui->memoryLabel->setText(pending > 0
                                     ? QStringLiteral("●  记忆暂存中 · %1 条待同步").arg(pending)
                                     : QStringLiteral("●  记忆暂存中"));
        ui->memoryLabel->setToolTip(QStringLiteral("数据库暂不可用，对话会先暂存，连接恢复后自动补齐。"));
    }
    refreshMemoryStyle();
}

void ChatPage::refreshMemoryStyle()
{
    if (!ui->memoryLabel)
        return;
    ui->memoryLabel->style()->unpolish(ui->memoryLabel);
    ui->memoryLabel->style()->polish(ui->memoryLabel);
}

bool ChatPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const bool enter = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;
        if (enter && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            sendCurrent();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

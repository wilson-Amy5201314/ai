#include "RolePage.h"
#include "ui_RolePage.h"

#include "ChoiceCard.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLayoutItem>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace {
QString roleDescription(const QString &roleKey)
{
    if (roleKey == QStringLiteral("friend"))
        return QStringLiteral("亲近但有分寸，损你也站你");
    if (roleKey == QStringLiteral("teacher"))
        return QStringLiteral("过来人的稳，帮你看远一点");
    if (roleKey == QStringLiteral("partner"))
        return QStringLiteral("把你放心上，也把你照顾好");
    if (roleKey == QStringLiteral("father"))
        return QStringLiteral("话不多，关键时候靠得住");
    if (roleKey == QStringLiteral("mother"))
        return QStringLiteral("嘴上唠叨，手里全是照顾");
    return QStringLiteral("以这个身份陪你说说话");
}

void clearLayoutRecursive(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget())
            delete w;
        if (QLayout *child = item->layout()) {
            clearLayoutRecursive(child);
        }
        delete item;
    }
}
} // namespace

RolePage::RolePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RolePage)
{
    ui->setupUi(this);

    connect(ui->backButton, &QPushButton::clicked, this, &RolePage::backRequested);
    m_createButton = new QPushButton(QStringLiteral("＋  创建角色"), this);
    m_createButton->setObjectName(QStringLiteral("ghost"));
    m_createButton->setMinimumSize(132, 42);
    ui->footerRow->insertWidget(1, m_createButton);
    connect(m_createButton, &QPushButton::clicked, this, &RolePage::createRoleRequested);

    m_deleteButton = new QPushButton(QStringLiteral("删除角色"), this);
    m_deleteButton->setObjectName(QStringLiteral("ghost"));
    m_deleteButton->setMinimumSize(100, 42);
    m_deleteButton->setEnabled(false);
    ui->footerRow->insertWidget(2, m_deleteButton);
    connect(m_deleteButton, &QPushButton::clicked, this, [this] {
        if (m_selected >= 0 && m_selected < m_personas.size())
            emit deleteRoleRequested(m_personas.at(m_selected));
    });
    connect(ui->continueButton, &QPushButton::clicked, this, [this] {
        if (m_selected >= 0 && m_selected < m_personas.size())
            emit personaSelected(m_personas.at(m_selected));
    });
}

RolePage::~RolePage()
{
    delete ui;
}

void RolePage::setPersonas(const QVector<Persona> &personas, const QString &gender)
{
    m_personas = personas;
    m_gender = gender;
    m_selected = -1;
    m_launching = false;
    updateIdentityChip();
    ui->subLabel->setText(personas.isEmpty()
                              ? QStringLiteral("还没有角色。先创建一个，再开始对话。")
                              : QStringLiteral("每一种关系，都会用不同的方式接住你。"));
    ui->continueButton->setText(QStringLiteral("和 ta 聊聊  →"));
    ui->continueButton->setEnabled(false);
    rebuildCards();
    setCreateEnabled(personas.size() < 5);
    m_deleteButton->setEnabled(false);
}

void RolePage::setProfileAge(int age, const QString &name)
{
    m_age = age;
    m_profileName = name.trimmed();
    if (!m_gender.isEmpty())
        updateIdentityChip();
}

void RolePage::updateIdentityChip()
{
    const QString who = m_profileName.isEmpty()
                            ? (m_gender == QStringLiteral("male") ? QStringLiteral("男生")
                                                                  : QStringLiteral("女生"))
                            : m_profileName;
    ui->genderChip->setText(m_age > 0
                                ? QStringLiteral("你以「%1 · %2 岁」的身份进入").arg(who).arg(m_age)
                                : QStringLiteral("你以「%1」的身份进入").arg(who));
}

void RolePage::setCreateEnabled(bool enabled)
{
    if (!m_createButton)
        return;
    m_createButton->setEnabled(enabled);
    m_createButton->setText(enabled ? QStringLiteral("＋  创建角色")
                                    : QStringLiteral("已达 5 个上限"));
    m_createButton->setToolTip(enabled ? QString()
                                       : QStringLiteral("每个用户资料最多 5 个关系角色，请先删除一个。"));
}

void RolePage::setLaunching(bool launching)
{
    m_launching = launching;
    ui->continueButton->setEnabled(!m_launching && m_selected >= 0);
    m_deleteButton->setEnabled(!m_launching && m_selected >= 0);
}

void RolePage::clearSelection()
{
    m_selected = -1;
    m_launching = false;
    for (ChoiceCard *card : std::as_const(m_cards))
        card->setSelected(false);
    ui->continueButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
}

void RolePage::selectPersona(const QString &personaKey)
{
    for (int i = 0; i < m_personas.size(); ++i) {
        if (m_personas.at(i).key == personaKey) {
            selectIndex(i);
            return;
        }
    }
}

void RolePage::rebuildCards()
{
    clearLayoutRecursive(ui->cardsLayout);
    m_cards.clear();

    if (m_personas.isEmpty())
        return;

    int index = 0;
    for (int row = 0; row < 2; ++row) {
        auto *line = new QHBoxLayout;
        line->setSpacing(14);
        line->addStretch(1);
        const int count = (row == 0) ? qMin(3, m_personas.size()) : qMax(0, m_personas.size() - 3);
        for (int i = 0; i < count; ++i) {
            const Persona &p = m_personas.at(index);
            const int cardIndex = index;
            const QString subtitle = p.tagline.isEmpty() ? roleDescription(p.roleKey) : p.tagline;
            auto *card = new ChoiceCard(p.avatar.isEmpty() ? Theme::roleEmoji(p.roleKey) : p.avatar,
                                        p.displayName.isEmpty() ? p.roleName : p.displayName,
                                        subtitle, QString(),
                                        Theme::roleAccent(p.roleKey), QSize(250, 200),
                                        ui->cardsHost);
            card->setRoleInfo(p.relation.isEmpty() ? p.roleName : p.relation,
                              p.roleAge, p.roleGender, p.personality, p.name);
            card->setFlipEnabled(true);
            connect(card, &ChoiceCard::clicked, this, [this, cardIndex] { selectIndex(cardIndex); });
            const QString personaKey = p.key;
            connect(card, &ChoiceCard::nameSubmitted, this,
                    [this, personaKey](const QString &name) {
                        emit roleNameSubmitted(personaKey, name);
                    });
            line->addWidget(card);
            m_cards.append(card);
            ++index;
        }
        line->addStretch(1);
        ui->cardsLayout->addLayout(line);
    }
    ui->cardsLayout->addStretch(1);
}

void RolePage::selectIndex(int index)
{
    if (index < 0 || index >= m_personas.size())
        return;
    if (index == m_selected) {
        clearSelection();
        return;
    }
    m_selected = index;
    for (int i = 0; i < m_cards.size(); ++i)
        m_cards.at(i)->setSelected(i == index);
    ui->continueButton->setEnabled(!m_launching);
    m_deleteButton->setEnabled(!m_launching);
}

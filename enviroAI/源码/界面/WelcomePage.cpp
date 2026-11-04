#include "WelcomePage.h"
#include "ui_WelcomePage.h"

#include "AgeStepper.h"
#include "ChoiceCard.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

WelcomePage::WelcomePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WelcomePage)
{
    ui->setupUi(this);
    ui->mainRow->setStretch(0, 1);
    ui->mainRow->setAlignment(ui->glassPanel, Qt::AlignVCenter);
    ui->glassPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    ui->maleCard->setContent(QStringLiteral("👦"), QStringLiteral("男生"),
                             QStringLiteral("以他的视角，被理解"), QString());
    ui->maleCard->setAccent(Theme::Mint);
    ui->maleCard->setCardSize(QSize(199, 228));

    ui->femaleCard->setContent(QStringLiteral("👧"), QStringLiteral("女生"),
                               QStringLiteral("以她的视角，被接住"), QString());
    ui->femaleCard->setAccent(Theme::Coral);
    ui->femaleCard->setCardSize(QSize(199, 228));

    auto *nameRow = new QHBoxLayout;
    nameRow->setSpacing(10);
    auto *nameLabel = new QLabel(QStringLiteral("你的名字（选填）"), this);
    nameLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("nameInput"));
    m_nameEdit->setPlaceholderText(QStringLiteral("例如：小林、阿杰"));
    m_nameEdit->setMaxLength(20);
    m_nameEdit->setClearButtonEnabled(true);
    nameRow->addWidget(nameLabel);
    nameRow->addSpacing(8);
    nameRow->addWidget(m_nameEdit, 1);
    ui->panelLayout->insertLayout(3, nameRow);

    auto *ageRow = new QHBoxLayout;
    ageRow->setSpacing(14);
    auto *ageCol = new QVBoxLayout;
    ageCol->setSpacing(2);
    auto *ageLabel = new QLabel(QStringLiteral("你的年龄"), this);
    ageLabel->setObjectName(QStringLiteral("fieldLabel"));
    ageCol->addWidget(ageLabel);
    m_ageStepper = new AgeStepper(this);
    m_ageStepper->setObjectName(QStringLiteral("welcomeAgeStepper"));
    m_ageStepper->setRange(6, 60);
    m_ageStepper->setValue(24);
    m_ageStepper->setFixedWidth(190);
    connect(m_ageStepper, &AgeStepper::outOfRange, this,
            [this](int value, int minimum, int maximum) {
                QMessageBox::warning(this, QStringLiteral("年龄超出范围"),
                                     QStringLiteral("用户年龄需要在 %1–%2 岁之间，%3 岁无效。")
                                         .arg(minimum)
                                         .arg(maximum)
                                         .arg(value));
            });
    ageRow->addLayout(ageCol);
    ageRow->addStretch(1);
    ageRow->addWidget(m_ageStepper);
    ui->panelLayout->insertLayout(4, ageRow);

    m_limitLabel = new QLabel(QStringLiteral(
        "用户角色已满 5 个。请在左侧删除一个资料，再创建新的性别/年龄。"), this);
    m_limitLabel->setObjectName(QStringLiteral("notice"));
    m_limitLabel->setWordWrap(true);
    m_limitLabel->setVisible(false);
    ui->panelLayout->insertWidget(5, m_limitLabel);

    ui->continueButton->setEnabled(false);
    ui->memoryLabel->setObjectName(QStringLiteral("memoryOffline"));

    connect(ui->maleCard, &ChoiceCard::clicked, this, [this] { choose(QStringLiteral("male")); });
    connect(ui->femaleCard, &ChoiceCard::clicked, this, [this] { choose(QStringLiteral("female")); });
    connect(ui->continueButton, &QPushButton::clicked, this, [this] {
        if (!m_gender.isEmpty())
            emit continueRequested();
    });
}

WelcomePage::~WelcomePage()
{
    delete ui;
}

void WelcomePage::choose(const QString &gender)
{
    m_gender = gender;
    ui->maleCard->setSelected(gender == QStringLiteral("male"));
    ui->femaleCard->setSelected(gender == QStringLiteral("female"));
    setContinueEnabled(true);
    emit genderSelected(gender);
}

void WelcomePage::setContinueEnabled(bool enabled)
{
    ui->continueButton->setEnabled(enabled && !m_gender.isEmpty());
}

void WelcomePage::setProfileLimitReached(bool reached)
{
    if (m_limitLabel)
        m_limitLabel->setVisible(reached);
}

void WelcomePage::selectGender(const QString &gender)
{
    choose(gender);
}

void WelcomePage::selectAge(int age)
{
    if (m_ageStepper)
        m_ageStepper->setValue(qBound(6, age, 60));
}

void WelcomePage::selectName(const QString &name)
{
    if (m_nameEdit)
        m_nameEdit->setText(name.trimmed().left(20));
}

void WelcomePage::focusNameInput()
{
    if (!m_nameEdit)
        return;
    m_nameEdit->setFocus(Qt::OtherFocusReason);
    m_nameEdit->setCursorPosition(m_nameEdit->text().size());
}

QString WelcomePage::selectedName() const
{
    return m_nameEdit ? m_nameEdit->text().trimmed().left(20) : QString();
}

int WelcomePage::selectedAge() const
{
    return m_ageStepper ? m_ageStepper->value() : 24;
}

void WelcomePage::setMemoryStatus(bool online, int pending)
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
    ui->memoryLabel->style()->unpolish(ui->memoryLabel);
    ui->memoryLabel->style()->polish(ui->memoryLabel);
}

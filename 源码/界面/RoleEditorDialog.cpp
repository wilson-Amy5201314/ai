#include "RoleEditorDialog.h"

#include "AgeStepper.h"
#include "RoleUtils.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
QLabel *makeMuted(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("muted"));
    label->setWordWrap(true);
    return label;
}

int defaultRoleAge(const QString &relation, int userAge)
{
    if (relationLooksParent(relation))
        return qMin(100, userAge + 25);
    if (relationLooksSibling(relation))
        return qMin(100, userAge + 3);
    if (relationLooksTeacher(relation))
        return qMin(100, qMax(18, userAge + 8));
    if (relationLooksLover(relation))
        return qMin(100, qMax(18, userAge + 2));
    return qMin(100, qMax(1, userAge));
}

int minimumRoleAge(const QString &relation, int userAge)
{
    if (relationLooksParent(relation))
        return qMin(100, userAge + 22);
    if (relationLooksTeacher(relation))
        return qMin(100, qMax(18, userAge));
    if (relationLooksLover(relation))
        return 18;
    return 1;
}
} // namespace

RoleEditorDialog::RoleEditorDialog(const UserProfile &profile, QWidget *parent)
    : QDialog(parent)
    , m_profile(profile)
{
    setWindowTitle(QStringLiteral("创建一个角色"));
    setModal(true);
    resize(590, 520);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(34, 28, 34, 26);
    outer->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("先说说，ta 是你的谁？"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    outer->addWidget(title);

    auto *sub = new QLabel(QStringLiteral("先只选关系，下一步再补 ta 的年龄、性格和名字。"));
    sub->setObjectName(QStringLiteral("sectionSub"));
    outer->addWidget(sub);

    m_pages = new QStackedWidget(this);
    outer->addWidget(m_pages, 1);

    // 第一步：只出现关系选择。
    auto *relationPage = new QWidget(m_pages);
    auto *relationLayout = new QVBoxLayout(relationPage);
    relationLayout->setContentsMargins(0, 14, 0, 0);
    relationLayout->setSpacing(10);

    auto *relationLabel = new QLabel(QStringLiteral("关系"));
    relationLabel->setObjectName(QStringLiteral("fieldLabel"));
    relationLayout->addWidget(relationLabel);

    m_relationCombo = new QComboBox(relationPage);
    m_relationCombo->setObjectName(QStringLiteral("relationCombo"));
    m_relationCombo->addItem(QStringLiteral("请选择关系"), QString());
    m_relationCombo->addItem(QStringLiteral("老师"), QStringLiteral("老师"));
    m_relationCombo->addItem(QStringLiteral("朋友"), QStringLiteral("朋友"));
    m_relationCombo->addItem(QStringLiteral("恋人"), QStringLiteral("恋人"));
    m_relationCombo->addItem(QStringLiteral("父母"), QStringLiteral("父母"));
    m_relationCombo->addItem(QStringLiteral("兄弟姐妹"), QStringLiteral("兄弟姐妹"));
    m_relationCombo->addItem(QStringLiteral("亲戚"), QStringLiteral("亲戚"));
    relationLayout->addWidget(m_relationCombo);
    relationLayout->addWidget(makeMuted(
        QStringLiteral("选项：老师 · 朋友 · 恋人 · 父母 · 兄弟姐妹 · 亲戚")));
    relationLayout->addWidget(makeMuted(
        QStringLiteral("父母会在下一步根据角色性别细化为父亲/母亲；兄弟姐妹会根据性别和年龄细化为哥哥/弟弟/姐姐/妹妹。")));

    relationLayout->addStretch(1);
    auto *nextRow = new QHBoxLayout;
    nextRow->addStretch(1);
    m_nextButton = new QPushButton(QStringLiteral("下一步  →"), relationPage);
    m_nextButton->setObjectName(QStringLiteral("primary"));
    m_nextButton->setMinimumSize(150, 44);
    m_nextButton->setEnabled(false);
    nextRow->addWidget(m_nextButton);
    relationLayout->addLayout(nextRow);
    m_pages->addWidget(relationPage);

    // 第二步：补全资料。
    auto *detailPage = new QWidget(m_pages);
    auto *detailLayout = new QVBoxLayout(detailPage);
    detailLayout->setContentsMargins(0, 8, 0, 0);
    detailLayout->setSpacing(12);

    m_relationChip = new QLabel(detailPage);
    m_relationChip->setObjectName(QStringLiteral("roleChip"));
    m_relationChip->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    detailLayout->addWidget(m_relationChip);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);

    m_genderCombo = new QComboBox(detailPage);
    m_genderCombo->addItem(QStringLiteral("请选择"), QString());
    m_genderCombo->addItem(QStringLiteral("男"), QStringLiteral("male"));
    m_genderCombo->addItem(QStringLiteral("女"), QStringLiteral("female"));
    form->addRow(QStringLiteral("角色性别 *"), m_genderCombo);

    m_ageStepper = new AgeStepper(detailPage);
    m_ageStepper->setRange(1, 100);
    m_ageStepper->setValue(30);
    m_ageStepper->setFixedWidth(180);
    connect(m_ageStepper, &AgeStepper::outOfRange, this,
            [this](int value, int minimum, int maximum) {
                QMessageBox::warning(this, QStringLiteral("年龄超出范围"),
                                     QStringLiteral("关系角色年龄需要在 %1–%2 岁之间，%3 岁无效。")
                                         .arg(minimum)
                                         .arg(maximum)
                                         .arg(value));
            });
    form->addRow(QStringLiteral("角色年龄 *"), m_ageStepper);

    m_personalityEdit = new QLineEdit(detailPage);
    m_personalityEdit->setPlaceholderText(QStringLiteral("例如：温柔、古板、开朗…"));
    form->addRow(QStringLiteral("性格 *"), m_personalityEdit);

    m_nameEdit = new QLineEdit(detailPage);
    m_nameEdit->setPlaceholderText(QStringLiteral("选填，例如：阿杰、小满"));
    m_nameEdit->setMaxLength(20);
    form->addRow(QStringLiteral("名字（选填）"), m_nameEdit);
    detailLayout->addLayout(form);

    detailLayout->addWidget(makeMuted(
        QStringLiteral("性格示例：温柔 · 古板 · 开朗 · 细心 · 强势 · 慢热 · 幽默")));
    detailLayout->addWidget(makeMuted(
        QStringLiteral("父母年龄不能低于用户年龄 + 22 岁；老师、恋人年龄不低于 18 岁；兄弟姐妹不能与用户同岁。")));

    detailLayout->addStretch(1);
    auto *buttonRow = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("←  上一步"), detailPage);
    back->setObjectName(QStringLiteral("ghost"));
    back->setMinimumSize(112, 42);
    buttonRow->addWidget(back);
    buttonRow->addStretch(1);
    auto *create = new QPushButton(QStringLiteral("生成角色  →"), detailPage);
    create->setObjectName(QStringLiteral("primary"));
    create->setMinimumSize(160, 44);
    buttonRow->addWidget(create);
    detailLayout->addLayout(buttonRow);
    m_pages->addWidget(detailPage);

    connect(m_relationCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_nextButton->setEnabled(index > 0);
        if (index > 0) {
            const QString relation = m_relationCombo->currentData().toString();
            updateAgeRange(relation);
            m_ageStepper->setValue(defaultRoleAge(relation, m_profile.age));
        }
        updateRelationChip();
    });
    connect(m_nextButton, &QPushButton::clicked, this, &RoleEditorDialog::goNext);
    connect(back, &QPushButton::clicked, this, &RoleEditorDialog::goBack);
    connect(create, &QPushButton::clicked, this, &RoleEditorDialog::accept);
    connect(m_genderCombo, &QComboBox::currentIndexChanged, this, [this] { updateRelationChip(); });
    connect(m_ageStepper, &AgeStepper::valueChanged, this, [this] { updateRelationChip(); });
}

RoleRequest RoleEditorDialog::request() const
{
    RoleRequest r;
    r.relation = refinedRelation(m_relationCombo->currentData().toString(),
                                 m_genderCombo->currentData().toString(),
                                 m_ageStepper->value(), m_profile.age);
    r.roleGender = m_genderCombo->currentData().toString();
    r.roleAge = m_ageStepper->value();
    r.personality = m_personalityEdit->text().trimmed();
    r.name = m_nameEdit->text().trimmed();
    r.userGender = m_profile.gender;
    r.userAge = m_profile.age;
    return r;
}

void RoleEditorDialog::updateRelationChip()
{
    if (!m_relationChip)
        return;
    const QString base = m_relationCombo->currentData().toString();
    if (base.isEmpty()) {
        m_relationChip->setText(QStringLiteral("关系：未选择"));
        return;
    }
    const QString gender = m_genderCombo->currentData().toString();
    if ((base == QStringLiteral("父母") || base == QStringLiteral("兄弟姐妹")) && gender.isEmpty()) {
        m_relationChip->setText(QStringLiteral("关系：%1（请选择角色性别）").arg(base));
        return;
    }
    if (base == QStringLiteral("兄弟姐妹") && m_ageStepper->value() == m_profile.age) {
        m_relationChip->setText(QStringLiteral("关系：兄弟姐妹（请调整年龄，不能与用户同岁）"));
        return;
    }
    const QString refined = refinedRelation(base, gender, m_ageStepper->value(), m_profile.age);
    m_relationChip->setText(QStringLiteral("关系：%1").arg(refined));
}

void RoleEditorDialog::goNext()
{
    if (m_relationCombo->currentData().toString().isEmpty())
        return;

    const QString relation = m_relationCombo->currentData().toString();
    updateAgeRange(relation);
    m_ageStepper->setValue(defaultRoleAge(relation, m_profile.age));
    updateRelationChip();
    m_pages->setCurrentIndex(1);
    m_personalityEdit->setFocus();
}

void RoleEditorDialog::updateAgeRange(const QString &relation)
{
    const int minimum = minimumRoleAge(relation, m_profile.age);
    m_ageStepper->setRange(minimum, 100);
}

void RoleEditorDialog::goBack()
{
    m_pages->setCurrentIndex(0);
    m_relationCombo->setFocus();
}

void RoleEditorDialog::accept()
{
    if (m_pages->currentIndex() == 0) {
        goNext();
        return;
    }

    const RoleRequest r = request();
    const QString error = validateRoleRequest(r, m_profile);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("还差一点"), error);
        return;
    }
    QDialog::accept();
}

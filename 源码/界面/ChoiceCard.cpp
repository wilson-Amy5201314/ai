#include "ChoiceCard.h"

#include "Theme.h"

#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
ChoiceCard::ChoiceCard(QWidget *parent)
    : QWidget(parent)
    , m_accent(QColor(46, 110, 99))
{
    setFixedSize(300, 188);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);

    buildFront();
    buildBack();

    m_checkBadge = new QLabel(this);
    m_checkBadge->setObjectName(QStringLiteral("cardCheckBadge"));
    m_checkBadge->setFixedSize(22, 22);
    m_checkBadge->setAlignment(Qt::AlignCenter);
    m_checkBadge->setText(QStringLiteral("✓"));
    m_checkBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_checkBadge->hide();

    m_frontAnimation = new QPropertyAnimation(m_frontPage, "pos", this);
    m_frontAnimation->setDuration(1000);
    m_frontAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    m_backAnimation = new QPropertyAnimation(m_backPage, "pos", this);
    m_backAnimation->setDuration(1000);
    m_backAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    m_animation = new QParallelAnimationGroup(this);
    m_animation->addAnimation(m_frontAnimation);
    m_animation->addAnimation(m_backAnimation);

    m_checkBadge->move(width() - 30, 10);
    setAccent(m_accent);
}

ChoiceCard::ChoiceCard(const QString &emoji, const QString &title, const QString &subtitle,
                       const QString &meta, const QColor &accent, const QSize &cardSize,
                       QWidget *parent)
    : ChoiceCard(parent)
{
    setAccent(accent);
    setCardSize(cardSize);
    setContent(emoji, title, subtitle, meta);
}

void ChoiceCard::buildFront()
{
    m_frontPage = new QWidget(this);
    m_frontPage->setObjectName(QStringLiteral("cardFront"));
    m_frontPage->setStyleSheet(QStringLiteral("background: transparent;"));
    m_frontPage->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_frontPage->setGeometry(0, 0, width(), height());

    auto *root = new QVBoxLayout(m_frontPage);
    root->setContentsMargins(22, 19, 22, 18);
    root->setSpacing(3);

    m_emojiLabel = new QLabel(m_frontPage);
    m_emojiLabel->setStyleSheet(QStringLiteral("font-size: 38px; background: transparent;"));
    m_emojiLabel->setFixedHeight(48);
    m_emojiLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_titleLabel = new QLabel(m_frontPage);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 700; color: #172622; background: transparent;"));
    m_titleLabel->setMinimumHeight(28);
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_subtitleLabel = new QLabel(m_frontPage);
    m_subtitleLabel->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; background: transparent;"));
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->setMinimumHeight(34);
    m_subtitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_subtitleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_subtitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_metaLabel = new QLabel(m_frontPage);
    m_metaLabel->setWordWrap(true);
    m_metaLabel->setStyleSheet(QStringLiteral("font-size: 12px; color: #7B8A84; background: transparent;"));
    m_metaLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    root->addWidget(m_emojiLabel);
    root->addSpacing(2);
    root->addWidget(m_titleLabel);
    root->addWidget(m_subtitleLabel);
    root->addSpacing(4);
    root->addWidget(m_metaLabel);
    root->addStretch(1);
}

void ChoiceCard::buildBack()
{
    m_backPage = new QWidget(this);
    m_backPage->setObjectName(QStringLiteral("cardBack"));
    m_backPage->setStyleSheet(QStringLiteral("background: transparent;"));
    m_backPage->setGeometry(width(), 0, width(), height());

    auto *layout = new QVBoxLayout(m_backPage);
    layout->setContentsMargins(20, 10, 20, 10);
    layout->setSpacing(3);

    auto *title = new QLabel(QStringLiteral("关系角色"), m_backPage);
    title->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 700; color: #2E6E63; background: transparent;"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_backRelation = new QLabel(m_backPage);
    m_backAge = new QLabel(m_backPage);
    m_backGender = new QLabel(m_backPage);
    m_backPersonality = new QLabel(m_backPage);
    m_backName = new QLabel(m_backPage);
    for (QLabel *label : {m_backRelation, m_backAge, m_backGender, m_backName}) {
        label->setStyleSheet(QStringLiteral("font-size: 13px; color: #334B44; background: transparent;"));
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    m_backPersonality->setStyleSheet(QStringLiteral(
        "font-size: 12px; color: #334B44; background: transparent;"));
    m_backPersonality->setWordWrap(true);
    m_backPersonality->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *nameRow = new QHBoxLayout;
    nameRow->setSpacing(5);
    m_backNameLabel = new QLabel(QStringLiteral("名字："), m_backPage);
    m_backNameLabel->setStyleSheet(QStringLiteral("font-size: 13px; color: #334B44; background: transparent;"));
    m_backNameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_backNameEdit = new QLineEdit(m_backPage);
    m_backNameEdit->setObjectName(QStringLiteral("roleNameInput"));
    m_backNameEdit->setPlaceholderText(QStringLiteral("输入名字"));
    m_backNameEdit->setMaxLength(20);
    m_backNameEdit->setMaximumWidth(92);
    m_backNameButton = new QPushButton(QStringLiteral("确认"), m_backPage);
    m_backNameButton->setObjectName(QStringLiteral("ghost"));
    m_backNameButton->setFixedSize(58, 30);
    m_backNameButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 12px; padding: 0 4px; }"));
    m_backNameButton->setToolTip(QStringLiteral("确认名字"));
    nameRow->addWidget(m_backNameLabel);
    nameRow->addWidget(m_backName);
    nameRow->addWidget(m_backNameEdit);
    nameRow->addWidget(m_backNameButton);
    nameRow->addStretch(1);

    m_backNameHint = new QLabel(QStringLiteral("只能设置一次"), m_backPage);
    m_backNameHint->setObjectName(QStringLiteral("muted"));
    m_backNameHint->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: #8C9A94; background: transparent;"));
    m_backNameHint->setAttribute(Qt::WA_TransparentForMouseEvents);

    layout->addWidget(title);
    layout->addSpacing(2);
    layout->addWidget(m_backRelation);
    layout->addWidget(m_backAge);
    layout->addWidget(m_backGender);
    layout->addWidget(m_backPersonality);
    layout->addLayout(nameRow);
    layout->addWidget(m_backNameHint);
    layout->addStretch(1);

    m_backPage->installEventFilter(this);
    connect(m_backNameButton, &QPushButton::clicked, this, &ChoiceCard::submitName);
    connect(m_backNameEdit, &QLineEdit::returnPressed, this, &ChoiceCard::submitName);

    m_backPage->raise();
}

void ChoiceCard::submitName()
{
    if (!m_backNameEdit)
        return;
    const QString name = m_backNameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_backNameEdit->setFocus();
        return;
    }
    emit nameSubmitted(name.left(20));
}

void ChoiceCard::setContent(const QString &emoji, const QString &title, const QString &subtitle,
                            const QString &meta)
{
    m_emojiLabel->setText(emoji);
    m_titleLabel->setText(title);
    m_subtitleLabel->setText(subtitle);
    m_metaLabel->setText(meta);
    m_metaLabel->setVisible(!meta.isEmpty());
}

void ChoiceCard::setRoleInfo(const QString &relation, int age, const QString &gender,
                             const QString &personality, const QString &name)
{
    m_backRelation->setText(QStringLiteral("关系：%1").arg(relation));
    m_backAge->setText(QStringLiteral("年龄：%1")
                           .arg(age > 0 ? QStringLiteral("%1 岁").arg(age)
                                        : QStringLiteral("未填写")));
    m_backGender->setText(QStringLiteral("性别：%1")
                              .arg(gender == QStringLiteral("male")
                                       ? QStringLiteral("男")
                                       : (gender == QStringLiteral("female")
                                              ? QStringLiteral("女")
                                              : QStringLiteral("未填写"))));
    m_backPersonality->setText(QStringLiteral("性格：%1")
                                   .arg(personality.trimmed().isEmpty()
                                            ? QStringLiteral("未填写")
                                            : personality.trimmed()));

    const QString trimmedName = name.trimmed();
    const bool hasName = !trimmedName.isEmpty();
    m_backName->setVisible(hasName);
    m_backName->setText(trimmedName);
    m_backNameEdit->setVisible(!hasName);
    m_backNameButton->setVisible(!hasName);
    m_backNameHint->setVisible(!hasName);
    if (!hasName)
        m_backNameEdit->clear();
}

void ChoiceCard::setFlipEnabled(bool enabled)
{
    m_flipEnabled = enabled;
    if (!enabled) {
        if (m_animation)
            m_animation->stop();
        if (m_frontPage)
            m_frontPage->move(0, 0);
        if (m_backPage)
            m_backPage->move(width(), 0);
    }
}

void ChoiceCard::setAccent(const QColor &accent)
{
    m_accent = accent;
    m_subtitleLabel->setStyleSheet(QStringLiteral(
        "font-size: 13px; font-weight: 600; color: %1; background: transparent;")
                                       .arg(m_accent.name()));
    if (m_checkBadge) {
        m_checkBadge->setStyleSheet(
            QStringLiteral("background: %1; border-radius: 11px; color: white; "
                           "font-size: 11px; font-weight: 700;")
                .arg(m_accent.name()));
    }
    update();
}

void ChoiceCard::setCardSize(const QSize &cardSize)
{
    setFixedSize(cardSize);
    updateGeometry();
}

void ChoiceCard::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    if (m_checkBadge) {
        m_checkBadge->setVisible(selected);
        if (selected)
            m_checkBadge->raise();
    }
    if (m_flipEnabled)
        startReveal(selected);
    update();
}

void ChoiceCard::startReveal(bool reveal)
{
    if (!m_frontPage || !m_backPage || !m_animation)
        return;
    m_animation->stop();
    m_frontAnimation->setStartValue(m_frontPage->pos());
    m_frontAnimation->setEndValue(QPoint(reveal ? -width() : 0, 0));
    m_backAnimation->setStartValue(m_backPage->pos());
    m_backAnimation->setEndValue(QPoint(reveal ? 0 : width(), 0));
    m_animation->start();
    if (m_checkBadge)
        m_checkBadge->raise();
}

void ChoiceCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_frontPage && m_backPage) {
        m_frontPage->resize(size());
        m_backPage->resize(size());
        if (!m_animation || m_animation->state() != QAbstractAnimation::Running) {
            m_frontPage->move(m_hover ? -width() : 0, 0);
            m_backPage->move(m_hover ? 0 : width(), 0);
        }
    }
    if (m_checkBadge)
        m_checkBadge->move(width() - 30, 10);
}

void ChoiceCard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(1.2, 1.2, -1.2, -1.2);
    const qreal radius = 22.0;

    QColor bg = m_hover ? QColor(255, 253, 249, 252) : QColor(255, 252, 247, 226);
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(r, radius, radius);

    if (m_selected) {
        QLinearGradient wash(r.bottomLeft(), r.topLeft());
        wash.setColorAt(0.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 22));
        wash.setColorAt(1.0, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 0));
        p.setBrush(wash);
        p.drawRoundedRect(r, radius, radius);
    }

    QPen pen;
    if (m_selected) {
        pen = QPen(QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 210), 2.0);
    } else if (m_hover) {
        pen = QPen(QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 100), 1.4);
    } else {
        pen = QPen(QColor(28, 43, 39, 26), 1.0);
    }
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, radius, radius);
}

void ChoiceCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void ChoiceCard::enterEvent(QEnterEvent *event)
{
    m_hover = true;
    update();
    QWidget::enterEvent(event);
}

void ChoiceCard::leaveEvent(QEvent *event)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(event);
}

void ChoiceCard::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit clicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool ChoiceCard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_backPage) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                // 接受按下事件，确保同一次普通点击能收到松开事件。
                m_backPressed = true;
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && m_backPressed) {
                m_backPressed = false;
                emit clicked();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

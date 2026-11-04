#pragma once

#include <QColor>
#include <QSize>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QEvent;
class QMouseEvent;
class QKeyEvent;
class QEnterEvent;
class QParallelAnimationGroup;
class QPropertyAnimation;

class ChoiceCard : public QWidget
{
    Q_OBJECT
public:
    explicit ChoiceCard(QWidget *parent = nullptr);
    ChoiceCard(const QString &emoji, const QString &title, const QString &subtitle,
               const QString &meta, const QColor &accent, const QSize &cardSize,
               QWidget *parent = nullptr);

    void setContent(const QString &emoji, const QString &title, const QString &subtitle,
                    const QString &meta);
    void setRoleInfo(const QString &relation, int age, const QString &gender,
                     const QString &personality, const QString &name);
    void setFlipEnabled(bool enabled);
    void setAccent(const QColor &accent);
    void setCardSize(const QSize &cardSize);
    void setSelected(bool selected);

signals:
    void clicked();
    void nameSubmitted(const QString &name);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildFront();
    void buildBack();
    void startReveal(bool reveal);
    void submitName();

    QWidget *m_frontPage = nullptr;
    QWidget *m_backPage = nullptr;
    QLabel *m_emojiLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QLabel *m_metaLabel = nullptr;
    QLabel *m_backRelation = nullptr;
    QLabel *m_backAge = nullptr;
    QLabel *m_backGender = nullptr;
    QLabel *m_backPersonality = nullptr;
    QLabel *m_backName = nullptr;
    QLabel *m_backNameLabel = nullptr;
    QLineEdit *m_backNameEdit = nullptr;
    QPushButton *m_backNameButton = nullptr;
    QLabel *m_backNameHint = nullptr;
    QLabel *m_checkBadge = nullptr;
    QParallelAnimationGroup *m_animation = nullptr;
    QPropertyAnimation *m_frontAnimation = nullptr;
    QPropertyAnimation *m_backAnimation = nullptr;
    QColor m_accent;
    bool m_selected = false;
    bool m_hover = false;
    bool m_flipEnabled = false;
    bool m_backPressed = false;
};

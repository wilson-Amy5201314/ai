#pragma once

#include "Models.h"

#include <QVector>
#include <QWidget>

namespace Ui {
class RolePage;
}

class ChoiceCard;
class QPushButton;

class RolePage : public QWidget
{
    Q_OBJECT
public:
    explicit RolePage(QWidget *parent = nullptr);
    ~RolePage() override;

    void setPersonas(const QVector<Persona> &personas, const QString &gender);
    void clearSelection();
    void selectPersona(const QString &personaKey);
    void setLaunching(bool launching);
    void setCreateEnabled(bool enabled);
    void setProfileAge(int age, const QString &name = QString());

signals:
    void backRequested();
    void createRoleRequested();
    void deleteRoleRequested(const Persona &persona);
    void roleNameSubmitted(const QString &personaKey, const QString &name);
    void personaSelected(const Persona &persona);

private:
    void rebuildCards();
    void updateIdentityChip();
    void selectIndex(int index);

    Ui::RolePage *ui = nullptr;
    QVector<Persona> m_personas;
    QVector<ChoiceCard *> m_cards;
    QPushButton *m_createButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    int m_selected = -1;
    bool m_launching = false;
    QString m_gender;
    QString m_profileName;
    int m_age = 0;
};

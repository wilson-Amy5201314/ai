#pragma once

#include "Models.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class AgeStepper;

class RoleEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RoleEditorDialog(const UserProfile &profile, QWidget *parent = nullptr);

    RoleRequest request() const;

protected:
    void accept() override;

private:
    void goNext();
    void goBack();
    void updateRelationChip();
    void updateAgeRange(const QString &relation);

    UserProfile m_profile;
    QStackedWidget *m_pages = nullptr;
    QComboBox *m_relationCombo = nullptr;
    QPushButton *m_nextButton = nullptr;
    QLabel *m_relationChip = nullptr;
    QComboBox *m_genderCombo = nullptr;
    AgeStepper *m_ageStepper = nullptr;
    QLineEdit *m_personalityEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
};

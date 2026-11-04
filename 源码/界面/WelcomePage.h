#pragma once

#include <QWidget>

namespace Ui {
class WelcomePage;
}

class AgeStepper;
class QLabel;
class QLineEdit;

class WelcomePage : public QWidget
{
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);
    ~WelcomePage() override;

    void setMemoryStatus(bool online, int pending);
    void selectGender(const QString &gender);
    void selectAge(int age);
    void selectName(const QString &name);
    void focusNameInput();
    void setContinueEnabled(bool enabled);
    void setProfileLimitReached(bool reached);
    QString selectedGender() const { return m_gender; }
    QString selectedName() const;
    int selectedAge() const;

signals:
    void genderSelected(const QString &gender);
    void continueRequested();

private:
    void choose(const QString &gender);

    Ui::WelcomePage *ui = nullptr;
    QString m_gender;
    AgeStepper *m_ageStepper = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLabel *m_limitLabel = nullptr;
};

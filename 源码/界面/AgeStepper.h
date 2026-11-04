#pragma once

#include <QWidget>

class QEvent;
class QLabel;
class QLineEdit;
class QToolButton;

class AgeStepper : public QWidget
{
    Q_OBJECT
public:
    explicit AgeStepper(QWidget *parent = nullptr);

    void setRange(int minimum, int maximum);
    void setValue(int value);
    int value() const { return m_value; }

signals:
    void valueChanged(int value);
    void outOfRange(int value, int minimum, int maximum);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void step(int delta);
    void refresh();
    void commitInput();

    int m_minimum = 1;
    int m_maximum = 120;
    int m_value = 24;
    QLineEdit *m_valueEdit = nullptr;
    QLabel *m_suffix = nullptr;
    QToolButton *m_minus = nullptr;
    QToolButton *m_plus = nullptr;
};

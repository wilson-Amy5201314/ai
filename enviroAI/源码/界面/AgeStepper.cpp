#include "AgeStepper.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

AgeStepper::AgeStepper(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ageStepper"));
    setFixedHeight(54);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(7, 5, 7, 5);
    layout->setSpacing(6);

    m_minus = new QToolButton(this);
    m_minus->setObjectName(QStringLiteral("stepperButton"));
    m_minus->setText(QStringLiteral("−"));
    m_minus->setFixedSize(38, 38);
    m_minus->setCursor(Qt::PointingHandCursor);
    m_minus->setToolTip(QStringLiteral("减小一岁"));

    m_valueEdit = new QLineEdit(this);
    m_valueEdit->setObjectName(QStringLiteral("stepperValue"));
    m_valueEdit->setAlignment(Qt::AlignCenter);
    m_valueEdit->setFrame(false);
    m_valueEdit->setMinimumWidth(46);
    m_valueEdit->setMaximumWidth(92);
    m_valueEdit->setToolTip(QStringLiteral("点击数字后直接输入年龄"));
    m_valueEdit->setValidator(new QIntValidator(0, 9999, this));
    m_valueEdit->installEventFilter(this);

    m_suffix = new QLabel(QStringLiteral("岁"), this);
    m_suffix->setObjectName(QStringLiteral("stepperSuffix"));

    m_plus = new QToolButton(this);
    m_plus->setObjectName(QStringLiteral("stepperButton"));
    m_plus->setText(QStringLiteral("+"));
    m_plus->setFixedSize(38, 38);
    m_plus->setCursor(Qt::PointingHandCursor);
    m_plus->setToolTip(QStringLiteral("增加一岁"));

    layout->addWidget(m_minus);
    layout->addWidget(m_valueEdit, 1);
    layout->addWidget(m_suffix);
    layout->addWidget(m_plus);

    connect(m_minus, &QToolButton::clicked, this, [this] { step(-1); });
    connect(m_plus, &QToolButton::clicked, this, [this] { step(1); });
    connect(m_valueEdit, &QLineEdit::editingFinished, this, &AgeStepper::commitInput);
    connect(m_valueEdit, &QLineEdit::returnPressed, this, &AgeStepper::commitInput);
    refresh();
}

void AgeStepper::setRange(int minimum, int maximum)
{
    m_minimum = qMin(minimum, maximum);
    m_maximum = qMax(minimum, maximum);
    if (m_valueEdit)
        m_valueEdit->setValidator(new QIntValidator(0, 9999, this));
    setValue(m_value);
}

void AgeStepper::setValue(int value)
{
    const int clamped = qBound(m_minimum, value, m_maximum);
    if (clamped == m_value) {
        refresh();
        return;
    }
    m_value = clamped;
    refresh();
    emit valueChanged(m_value);
}

void AgeStepper::step(int delta)
{
    setValue(m_value + delta);
}

void AgeStepper::refresh()
{
    if (!m_valueEdit)
        return;
    QSignalBlocker blocker(m_valueEdit);
    m_valueEdit->setText(QString::number(m_value));
    m_valueEdit->setCursorPosition(m_valueEdit->text().size());
    if (m_suffix)
        m_suffix->setText(QStringLiteral("岁"));
    if (m_minus)
        m_minus->setEnabled(m_value > m_minimum);
    if (m_plus)
        m_plus->setEnabled(m_value < m_maximum);
}

void AgeStepper::commitInput()
{
    if (!m_valueEdit)
        return;
    bool ok = false;
    const int entered = m_valueEdit->text().trimmed().toInt(&ok);
    if (!ok) {
        refresh();
        return;
    }
    if (entered < m_minimum || entered > m_maximum) {
        emit outOfRange(entered, m_minimum, m_maximum);
        setValue(entered);
        refresh();
        return;
    }
    setValue(entered);
    refresh();
}

bool AgeStepper::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_valueEdit && event->type() == QEvent::FocusIn) {
        QTimer::singleShot(0, m_valueEdit, [this] {
            if (m_valueEdit)
                m_valueEdit->selectAll();
        });
    }
    return QWidget::eventFilter(watched, event);
}

#include "Theme.h"

QString Theme::fontFamily()
{
    return QStringLiteral("Microsoft YaHei UI");
}

QColor Theme::roleAccent(const QString &roleKey)
{
    if (roleKey == QStringLiteral("friend"))
        return QColor(78, 140, 122);
    if (roleKey == QStringLiteral("teacher"))
        return QColor(91, 127, 166);
    if (roleKey == QStringLiteral("partner"))
        return QColor(201, 120, 114);
    if (roleKey == QStringLiteral("father"))
        return QColor(176, 138, 85);
    if (roleKey == QStringLiteral("mother"))
        return QColor(154, 110, 143);
    return Mint;
}

QString Theme::roleEmoji(const QString &roleKey)
{
    if (roleKey == QStringLiteral("friend"))
        return QStringLiteral("🫂");
    if (roleKey == QStringLiteral("teacher"))
        return QStringLiteral("📖");
    if (roleKey == QStringLiteral("partner"))
        return QStringLiteral("💞");
    if (roleKey == QStringLiteral("father"))
        return QStringLiteral("🪵");
    if (roleKey == QStringLiteral("mother"))
        return QStringLiteral("🍲");
    return QStringLiteral("✨");
}

QString Theme::qss()
{
    return QStringLiteral(R"QSS(
QWidget {
    color: #1C2B27;
    font-size: 14px;
}
QLineEdit#roleNameInput {
    background: rgba(255, 255, 255, 0.86);
    border: 1px solid rgba(46, 110, 99, 0.20);
    border-radius: 12px;
    padding: 7px 10px;
    color: #172622;
    font-size: 13px;
}
QLineEdit#roleNameInput:focus {
    border: 1px solid rgba(46, 110, 99, 0.50);
}
QLineEdit#nameInput {
    background: rgba(255, 255, 255, 0.78);
    border: 1px solid rgba(46, 110, 99, 0.18);
    border-radius: 14px;
    padding: 9px 14px;
    font-size: 14px;
    color: #172622;
}
QLineEdit#nameInput:focus {
    border: 1px solid rgba(46, 110, 99, 0.48);
    background: rgba(255, 255, 255, 0.94);
}
QWidget#ageStepper, QWidget#welcomeAgeStepper {
    background: rgba(255, 255, 255, 0.72);
    border: 1px solid rgba(46, 110, 99, 0.20);
    border-radius: 22px;
}
QLineEdit#stepperValue {
    font-size: 20px;
    font-weight: 700;
    color: #143D36;
    letter-spacing: 1px;
    background: transparent;
    border: none;
    padding: 0;
    selection-background-color: rgba(46, 110, 99, 0.24);
}
QLabel#stepperSuffix {
    font-size: 13px;
    color: #6F7F79;
    background: transparent;
}
QToolButton#stepperButton {
    background: rgba(46, 110, 99, 0.10);
    border: none;
    border-radius: 17px;
    color: #2E6E63;
    font-size: 20px;
    font-weight: 600;
    padding: 0;
}
QToolButton#stepperButton:hover {
    background: rgba(46, 110, 99, 0.18);
}
QToolButton#stepperButton:pressed {
    background: rgba(46, 110, 99, 0.26);
}
QToolButton#stepperButton:disabled {
    color: #B6C0BB;
    background: rgba(46, 110, 99, 0.05);
}
QLabel#fieldLabel {
    font-size: 12px;
    color: #6F7F79;
}
QLineEdit, QSpinBox, QComboBox {
    background: rgba(255, 255, 255, 0.78);
    border: 1px solid rgba(46, 110, 99, 0.18);
    border-radius: 10px;
    padding: 8px 10px;
    color: #1C2B27;
    selection-background-color: #E8C4A8;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border-color: rgba(46, 110, 99, 0.55);
}
QComboBox QAbstractItemView {
    background: #FFFDF8;
    border: 1px solid rgba(46, 110, 99, 0.18);
    selection-background-color: rgba(46, 110, 99, 0.12);
}
QDockWidget#historyDock {
    background: rgba(255, 252, 247, 0.82);
    color: #2E4A43;
    border-right: 1px solid rgba(46, 110, 99, 0.12);
}
QDockWidget#historyDock::title {
    background: rgba(255, 252, 247, 0.95);
    color: #2E6E63;
    padding: 12px 14px;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 1px;
    border-bottom: 1px solid rgba(46, 110, 99, 0.10);
}
QWidget#historyHost {
    background: rgba(255, 252, 247, 0.88);
}
QLabel#historyTitle {
    font-size: 15px;
    font-weight: 700;
    color: #143D36;
    letter-spacing: 1px;
}
QLabel#historySubtitle {
    font-size: 11px;
    color: #8C9A94;
    padding-bottom: 4px;
}
QLabel#historyHint {
    font-size: 11px;
    color: #8C9A94;
    padding: 2px 0 4px 0;
}
QTreeWidget#historyTree {
    background: rgba(255, 252, 247, 0.82);
    border: none;
    color: #2E4A43;
    font-size: 12px;
    outline: 0;
}
QTreeWidget#historyTree::item {
    padding: 7px 5px;
    border-radius: 7px;
}
QTreeWidget#historyTree::item:hover {
    background: rgba(46, 110, 99, 0.06);
}
QTreeWidget#historyTree::item:selected {
    background: rgba(46, 110, 99, 0.12);
    color: #143D36;
}
QDialog {
    background: #F8F3EA;
}
QToolTip {
    background: #143D36;
    color: #F8F3EA;
    border: none;
    border-radius: 6px;
    padding: 6px 10px;
    font-size: 12px;
}
QLabel#brandName {
    font-size: 22px;
    font-weight: 700;
    letter-spacing: 3px;
    color: #143D36;
}
QLabel#brandMark {
    background: #143D36;
    color: #F8F3EA;
    border-radius: 12px;
    font-size: 20px;
    font-weight: 700;
}
QLabel#roleChip {
    font-size: 11px;
    color: #2E6E63;
    background: rgba(46, 110, 99, 0.10);
    border-radius: 10px;
    padding: 3px 9px;
}
QLabel#notice {
    font-size: 12px;
    color: #A47B5E;
    background: rgba(217, 132, 91, 0.08);
    border-radius: 12px;
    padding: 8px 16px;
}
QLabel#emptyTitle {
    font-size: 22px;
    font-weight: 600;
    color: #2E4A43;
}
QFrame#glassPanel {
    background: rgba(255, 252, 247, 0.72);
    border: 1px solid rgba(255, 255, 255, 0.85);
    border-radius: 30px;
}
QLabel#brandSub {
    font-size: 11px;
    letter-spacing: 2px;
    color: #8C9A94;
}
QLabel#eyebrow {
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 3px;
    color: #B07D5B;
}
QLabel#heroTitle {
    font-size: 44px;
    font-weight: 700;
    line-height: 1.25;
    color: #172622;
}
QLabel#heroSub {
    font-size: 15px;
    color: #5C6E68;
    line-height: 1.7;
}
QLabel#sectionTitle {
    font-size: 34px;
    font-weight: 700;
    color: #172622;
}
QLabel#sectionSub {
    font-size: 14px;
    color: #5C6E68;
}
QLabel#muted {
    font-size: 12px;
    color: #8C9A94;
}
QLabel#genderChip {
    font-size: 12px;
    color: #2E6E63;
    background: rgba(46, 110, 99, 0.10);
    border-radius: 13px;
    padding: 6px 14px;
}
QLabel#quote {
    font-size: 13px;
    color: #7B8A84;
    line-height: 1.7;
}
QLabel#memoryOnline {
    font-size: 11px;
    color: #2E6E63;
}
QLabel#memoryOffline {
    font-size: 11px;
    color: #A47B5E;
}
QPushButton#primary {
    background: #143D36;
    color: #F8F3EA;
    border: none;
    border-radius: 25px;
    padding: 14px 36px;
    font-size: 15px;
    font-weight: 600;
}
QPushButton#primary:hover { background: #1D5148; }
QPushButton#primary:pressed { background: #0F2F2A; }
QPushButton#primary:disabled { background: #D9E0DB; color: #93A19B; }
QPushButton#ghost {
    background: transparent;
    color: #2E6E63;
    border: 1px solid rgba(46, 110, 99, 0.22);
    border-radius: 17px;
    padding: 9px 20px;
    font-size: 13px;
}
QPushButton#ghost:hover { background: rgba(46, 110, 99, 0.07); }
QPushButton#ghost:disabled { color: #A9B4AF; border-color: rgba(28, 43, 39, 0.10); }
QPushButton#iconButton {
    background: rgba(255, 255, 255, 0.62);
    border: 1px solid rgba(28, 43, 39, 0.10);
    border-radius: 19px;
    font-size: 17px;
    color: #2E6E63;
}
QPushButton#iconButton:hover { background: #FFFFFF; }
QPushButton#sendButton {
    background: #143D36;
    color: #F8F3EA;
    border: none;
    border-radius: 23px;
    font-size: 21px;
    font-weight: 600;
}
QPushButton#sendButton:hover { background: #1D5148; }
QPushButton#sendButton:pressed { background: #0F2F2A; }
QPushButton#sendButton:disabled { background: #D9E0DB; color: #93A19B; }
QTextEdit#chatInput {
    background: transparent;
    border: none;
    color: #1C2B27;
    font-size: 15px;
    selection-background-color: #E8C4A8;
    selection-color: #172622;
}
QTextEdit#chatInput:focus { border: none; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
QScrollBar:vertical {
    background: transparent;
    width: 9px;
    margin: 4px 1px 4px 1px;
}
QScrollBar::handle:vertical {
    background: rgba(28, 43, 39, 0.16);
    border-radius: 4px;
    min-height: 28px;
}
QScrollBar::handle:vertical:hover { background: rgba(28, 43, 39, 0.28); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QFrame#composer {
    background: rgba(255, 252, 247, 0.90);
    border: 1px solid rgba(28, 43, 39, 0.10);
    border-radius: 24px;
}
QFrame#composer:focus-within { border: 1px solid rgba(46, 110, 99, 0.35); }
QFrame#headerLine {
    background: rgba(28, 43, 39, 0.08);
    border: none;
}
)QSS");
}

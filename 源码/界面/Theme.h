#pragma once

#include <QColor>
#include <QString>

namespace Theme {

inline const QColor Ink(28, 43, 39);
inline const QColor InkSoft(92, 110, 104);
inline const QColor InkFaint(140, 154, 148);
inline const QColor Cream(248, 243, 234);
inline const QColor Paper(255, 252, 247);
inline const QColor Mint(46, 110, 99);
inline const QColor MintDeep(20, 61, 54);
inline const QColor Coral(217, 132, 91);
inline const QColor Rose(201, 120, 114);
inline const QColor Gold(184, 145, 78);

QString fontFamily();
QString qss();
QColor roleAccent(const QString &roleKey);
QString roleEmoji(const QString &roleKey);

} // namespace Theme

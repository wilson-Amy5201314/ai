#pragma once

#include "Models.h"

#include <QString>
#include <QStringList>

inline bool relationContainsAny(const QString &relation, const QStringList &words)
{
    for (const QString &word : words) {
        if (relation.contains(word))
            return true;
    }
    return false;
}

inline bool relationLooksParent(const QString &relation)
{
    return relationContainsAny(relation, {QStringLiteral("父"), QStringLiteral("爸"),
                                          QStringLiteral("母"), QStringLiteral("妈")});
}

inline bool relationLooksTeacher(const QString &relation)
{
    return relationContainsAny(relation, {QStringLiteral("老师"), QStringLiteral("导师"),
                                          QStringLiteral("教授"), QStringLiteral("师父")});
}

inline bool relationLooksLover(const QString &relation)
{
    return relationContainsAny(relation, {QStringLiteral("恋人"), QStringLiteral("对象"),
                                          QStringLiteral("男友"), QStringLiteral("女友"),
                                          QStringLiteral("男朋友"), QStringLiteral("女朋友"),
                                          QStringLiteral("伴侣"), QStringLiteral("老公"),
                                          QStringLiteral("老婆")});
}

inline bool relationLooksSibling(const QString &relation)
{
    return relationContainsAny(relation, {QStringLiteral("兄弟姐妹"), QStringLiteral("哥哥"),
                                          QStringLiteral("弟弟"), QStringLiteral("姐姐"),
                                          QStringLiteral("妹妹")});
}

inline QString refinedRelation(const QString &relation, const QString &roleGender,
                               int roleAge, int userAge)
{
    const QString r = relation.trimmed();
    if (r.contains(QStringLiteral("父母")))
        return roleGender == QStringLiteral("male") ? QStringLiteral("父亲") : QStringLiteral("母亲");
    if (r.contains(QStringLiteral("兄弟姐妹"))) {
        if (roleAge > userAge)
            return roleGender == QStringLiteral("male") ? QStringLiteral("哥哥") : QStringLiteral("姐姐");
        if (roleAge < userAge)
            return roleGender == QStringLiteral("male") ? QStringLiteral("弟弟") : QStringLiteral("妹妹");
        return QStringLiteral("兄弟姐妹");
    }
    return r;
}

inline QString roleStyleKey(const QString &relation, const QString &roleGender)
{
    const QString r = relation.trimmed();
    if (r.contains(QStringLiteral("父母")))
        return roleGender == QStringLiteral("male") ? QStringLiteral("father") : QStringLiteral("mother");
    if (relationContainsAny(r, {QStringLiteral("朋友"), QStringLiteral("哥们"), QStringLiteral("闺蜜")}))
        return QStringLiteral("friend");
    if (relationLooksTeacher(r))
        return QStringLiteral("teacher");
    if (relationLooksLover(r))
        return QStringLiteral("partner");
    if (relationContainsAny(r, {QStringLiteral("父"), QStringLiteral("爸")}))
        return QStringLiteral("father");
    if (relationContainsAny(r, {QStringLiteral("母"), QStringLiteral("妈")}))
        return QStringLiteral("mother");
    return QStringLiteral("custom");
}

inline QString roleDisplayLabel(const QString &relation, const QString &name, int occurrence)
{
    QString label = relation.trimmed();
    if (occurrence > 1)
        label += QString::number(occurrence);
    const QString trimmedName = name.trimmed();
    if (!trimmedName.isEmpty())
        label += QStringLiteral("·") + trimmedName;
    return label;
}

inline QString validateRoleRequest(const RoleRequest &r, const UserProfile &profile)
{
    if (r.relation.trimmed().isEmpty())
        return QStringLiteral("请先填写关系。");
    if (r.roleGender != QStringLiteral("male") && r.roleGender != QStringLiteral("female"))
        return QStringLiteral("请选择角色性别。");
    if (r.roleAge < 1 || r.roleAge > 100)
        return QStringLiteral("角色年龄需要在 1–100 岁之间。");
    if (r.personality.trimmed().isEmpty())
        return QStringLiteral("请填写角色性格。");
    if (profile.age <= 0)
        return QStringLiteral("用户年龄资料无效。");

    const QString relation = r.relation.trimmed();
    const bool hasFather = relationContainsAny(relation, {QStringLiteral("父"), QStringLiteral("爸")});
    const bool hasMother = relationContainsAny(relation, {QStringLiteral("母"), QStringLiteral("妈")});
    if (hasFather && !hasMother && r.roleGender != QStringLiteral("male"))
        return QStringLiteral("父亲/爸爸的角色性别需要是男性。");
    if (hasMother && !hasFather && r.roleGender != QStringLiteral("female"))
        return QStringLiteral("母亲/妈妈的角色性别需要是女性。");

    if (relationLooksParent(relation) && r.roleAge < profile.age + 22)
        return QStringLiteral("父母的年龄不能低于用户年龄 + 22 岁。");
    if (relationLooksSibling(relation) && r.roleAge == profile.age)
        return QStringLiteral("兄弟姐妹的年龄不能和用户相同，请填写比用户大或小。");
    if (relationLooksTeacher(relation)) {
        if (r.roleAge < 18)
            return QStringLiteral("老师的年龄不能低于 18 岁。");
        if (r.roleAge < profile.age)
            return QStringLiteral("老师的年龄不应小于用户年龄。");
    }
    if (relationLooksLover(relation) && r.roleAge < 18)
        return QStringLiteral("恋人的年龄不能低于 18 岁。");

    return QString();
}

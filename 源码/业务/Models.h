#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

struct Persona
{
    QString key;
    QString roleKey;
    QString roleName;
    QString relation;
    QString name;
    QString roleGender;
    int roleAge = 0;
    QString personality;
    QString displayName;
    QString avatar;
    QString tagline;
    QString systemPrompt;

    bool isValid() const { return !key.isEmpty(); }
};

struct UserProfile
{
    QString id;
    QString gender;
    QString name;
    int age = 0;
    QDateTime createdAt;

    bool isValid() const { return !id.isEmpty() && !gender.isEmpty() && age > 0; }
    QString displayName() const
    {
        const QString trimmed = name.trimmed();
        if (!trimmed.isEmpty())
            return trimmed;
        return gender == QStringLiteral("male") ? QStringLiteral("男生") : QStringLiteral("女生");
    }
    QString listLabel() const
    {
        return age > 0 ? QStringLiteral("%1 · %2 岁").arg(displayName()).arg(age)
                       : displayName();
    }
};

struct RoleRequest
{
    QString relation;
    QString roleGender;
    int roleAge = 0;
    QString personality;
    QString name;
    QString userGender;
    int userAge = 0;

    bool isValid() const
    {
        return !relation.trimmed().isEmpty() && !roleGender.isEmpty() && roleAge > 0
               && !personality.trimmed().isEmpty() && !userGender.isEmpty() && userAge > 0;
    }
};

struct CustomRole
{
    QString id;
    QString profileId;
    QString relation;
    QString name;
    QString roleGender;
    int age = 0;
    QString personality;
    QString systemPrompt;
    QString tagline;
    QString avatar;
    QDateTime createdAt;

    bool isValid() const { return !id.isEmpty() && !relation.trimmed().isEmpty(); }
};

struct SessionInfo
{
    QString id;
    QDateTime createdAt;
    bool locked = false;
};

struct ChatTurn
{
    QString role; // user / assistant / system
    QString content;
    QDateTime at;

    bool isUser() const { return role == QStringLiteral("user"); }
    bool isAssistant() const { return role == QStringLiteral("assistant"); }
};

Q_DECLARE_METATYPE(Persona)

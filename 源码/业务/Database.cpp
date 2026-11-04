#include "Database.h"

#include "AppConfig.h"
#include "Facts.h"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace {
constexpr auto kConnectionName = "xinyu_sqlite";

QDateTime parseDbTime(const QString &text)
{
    QDateTime dt = QDateTime::fromString(text, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dt;
}
} // namespace

Database::Database(QObject *parent)
    : QObject(parent)
{
    loadPending();
}

Database::~Database()
{
    close();
}

bool Database::ping()
{
    if (!m_db.isOpen())
        return false;
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral("SELECT 1")) && q.next();
}

bool Database::open()
{
    if (m_online && ping())
        return true;

    const AppConfig &cfg = AppConfig::instance();
    m_lastError.clear();

    if (!QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE"))) {
        close();
        m_lastError = QStringLiteral("QSQLITE driver unavailable");
        qWarning() << "Database:" << m_lastError << QSqlDatabase::drivers();
        emit statusChanged(false, m_lastError);
        return false;
    }

    close();

    const QString path = cfg.sqliteFile.trimmed();
    if (path.isEmpty()) {
        m_lastError = QStringLiteral("SQLite path is empty");
        emit statusChanged(false, m_lastError);
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QString::fromLatin1(kConnectionName));
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "SQLite open failed:" << m_lastError << path;
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnectionName));
        emit statusChanged(false, m_lastError);
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));

    if (!ensureSchema()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnectionName));
        emit statusChanged(false, m_lastError);
        return false;
    }

    m_online = true;
    qInfo() << "SQLite connected:" << path;
    emit statusChanged(true, QStringLiteral("connected"));

    loadPending();
    const int flushed = pendingCount();
    if (flushed > 0 && flushPending())
        emit pendingFlushed(flushed);

    return true;
}

void Database::close()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    if (QSqlDatabase::contains(QString::fromLatin1(kConnectionName)))
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnectionName));
    if (m_online) {
        m_online = false;
        emit statusChanged(false, QStringLiteral("closed"));
    }
}

bool Database::ensureSchema()
{
    if (!m_db.isOpen())
        return false;
    return ensureSchemaFromResource();
}

bool Database::ensureSchemaFromResource()
{
    QFile f(QStringLiteral(":/database/schema.sql"));
    if (!f.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("schema resource missing");
        qWarning() << m_lastError;
        return false;
    }

    const QString raw = QString::fromUtf8(f.readAll());
    QString cleaned;
    const QStringList lines = raw.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("--")))
            continue;
        cleaned += line;
        cleaned += QLatin1Char('\n');
    }

    const QStringList statements = cleaned.split(QLatin1Char(';'));
    for (const QString &part : statements) {
        const QString sql = part.trimmed();
        if (sql.isEmpty())
            continue;
        QSqlQuery q(m_db);
        if (!q.exec(sql)) {
            m_lastError = q.lastError().text();
            qWarning() << "Schema statement failed:" << m_lastError << sql.left(120);
            return false;
        }
    }

    // 旧版 user_profiles 对 (gender, age) 建了唯一约束；新版允许同性别同年龄
    // 创建多个用户角色，因此需要重建表并保留原有资料。
    QSqlQuery meta(m_db);
    meta.prepare(QStringLiteral(
        "SELECT sql FROM sqlite_master WHERE type = 'table' AND name = 'user_profiles'"));
    if (meta.exec() && meta.next()) {
        const QString tableSql = meta.value(0).toString();
        if (tableSql.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive)) {
            meta.finish();
            if (!m_db.transaction()) {
                m_lastError = m_db.lastError().text();
                return false;
            }
            const QStringList migration = {
                QStringLiteral("ALTER TABLE user_profiles RENAME TO user_profiles_old"),
                QStringLiteral("CREATE TABLE user_profiles ("
                               "id TEXT PRIMARY KEY, gender TEXT NOT NULL, "
                               "name TEXT NOT NULL DEFAULT '', age INTEGER NOT NULL, "
                               "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
                QStringLiteral("INSERT INTO user_profiles (id, gender, age, created_at) "
                               "SELECT id, gender, age, created_at FROM user_profiles_old"),
                QStringLiteral("DROP TABLE user_profiles_old"),
            };
            for (const QString &statement : migration) {
                QSqlQuery q(m_db);
                if (!q.exec(statement)) {
                    m_lastError = q.lastError().text();
                    qWarning() << "Profile migration failed:" << m_lastError;
                    m_db.rollback();
                    return false;
                }
            }
            if (!m_db.commit()) {
                m_lastError = m_db.lastError().text();
                return false;
            }
            qInfo() << "Migrated user_profiles: removed unique(gender, age)";
        }
    }

    // 旧库没有 name 列时补上，空字符串表示使用“男生/女生 + 年龄”的默认显示。
    bool hasName = false;
    QSqlQuery columns(m_db);
    if (columns.exec(QStringLiteral("PRAGMA table_info(user_profiles)"))) {
        while (columns.next()) {
            if (columns.value(1).toString() == QStringLiteral("name")) {
                hasName = true;
                break;
            }
        }
    }
    columns.finish();
    if (!hasName) {
        QSqlQuery alter(m_db);
        if (!alter.exec(QStringLiteral(
                "ALTER TABLE user_profiles ADD COLUMN name TEXT NOT NULL DEFAULT ''"))) {
            m_lastError = alter.lastError().text();
            qWarning() << "Add user_profiles.name failed:" << m_lastError;
            return false;
        }
    }

    bool hasLocked = false;
    bool hasLockedAt = false;
    QSqlQuery sessionColumns(m_db);
    if (sessionColumns.exec(QStringLiteral("PRAGMA table_info(sessions)"))) {
        while (sessionColumns.next()) {
            const QString column = sessionColumns.value(1).toString();
            if (column == QStringLiteral("locked"))
                hasLocked = true;
            else if (column == QStringLiteral("locked_at"))
                hasLockedAt = true;
        }
    }
    sessionColumns.finish();
    if (!hasLocked) {
        QSqlQuery alter(m_db);
        if (!alter.exec(QStringLiteral(
                "ALTER TABLE sessions ADD COLUMN locked INTEGER NOT NULL DEFAULT 0"))) {
            m_lastError = alter.lastError().text();
            qWarning() << "Add sessions.locked failed:" << m_lastError;
            return false;
        }
    }
    if (!hasLockedAt) {
        QSqlQuery alter(m_db);
        if (!alter.exec(QStringLiteral("ALTER TABLE sessions ADD COLUMN locked_at TEXT"))) {
            m_lastError = alter.lastError().text();
            qWarning() << "Add sessions.locked_at failed:" << m_lastError;
            return false;
        }
    }
    return true;
}

UserProfile Database::ensureProfile(const QString &gender, int age, QString *error)
{
    return ensureProfile(gender, age, QString(), error);
}

UserProfile Database::ensureProfile(const QString &gender, int age, const QString &name,
                                    QString *error)
{
    UserProfile profile;
    if (!m_online) {
        if (error)
            *error = QStringLiteral("database offline");
        return profile;
    }
    if ((gender != QStringLiteral("male") && gender != QStringLiteral("female"))
        || age < 6 || age > 60) {
        if (error)
            *error = QStringLiteral("invalid profile");
        return profile;
    }

    // 同性别、同年龄也允许创建一个新的用户角色，不复用旧资料。
    if (profileCount() >= 5) {
        if (error)
            *error = QStringLiteral("user profile limit reached");
        return profile;
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO user_profiles (id, gender, name, age) VALUES (?, ?, ?, ?)"));
    insert.addBindValue(id);
    insert.addBindValue(gender);
    insert.addBindValue(name.trimmed().left(20));
    insert.addBindValue(age);
    if (!insert.exec()) {
        if (error)
            *error = insert.lastError().text();
        return profile;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, gender, name, age, created_at FROM user_profiles WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return profile;
    profile.id = q.value(0).toString();
    profile.gender = q.value(1).toString();
    profile.name = q.value(2).toString();
    profile.age = q.value(3).toInt();
    profile.createdAt = parseDbTime(q.value(4).toString());
    return profile;
}

QVector<UserProfile> Database::profiles(int limit)
{
    QVector<UserProfile> result;
    if (!m_online)
        return result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, gender, name, age, created_at FROM user_profiles "
        "ORDER BY created_at DESC, rowid DESC LIMIT ?"));
    q.addBindValue(qMax(1, limit));
    if (!q.exec())
        return result;
    while (q.next()) {
        UserProfile p;
        p.id = q.value(0).toString();
        p.gender = q.value(1).toString();
        p.name = q.value(2).toString();
        p.age = q.value(3).toInt();
        p.createdAt = parseDbTime(q.value(4).toString());
        if (p.isValid())
            result.append(p);
    }
    return result;
}

UserProfile Database::profileById(const QString &profileId)
{
    UserProfile profile;
    if (!m_online || profileId.isEmpty())
        return profile;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, gender, name, age, created_at FROM user_profiles WHERE id = ?"));
    q.addBindValue(profileId);
    if (!q.exec() || !q.next())
        return profile;
    profile.id = q.value(0).toString();
    profile.gender = q.value(1).toString();
    profile.name = q.value(2).toString();
    profile.age = q.value(3).toInt();
    profile.createdAt = parseDbTime(q.value(4).toString());
    return profile;
}

int Database::profileCount()
{
    if (!m_online)
        return 0;
    QSqlQuery q(QStringLiteral("SELECT COUNT(*) FROM user_profiles"), m_db);
    return q.next() ? q.value(0).toInt() : 0;
}

bool Database::deleteProfile(const QString &profileId)
{
    if (!m_online || profileId.isEmpty())
        return false;
    if (!m_db.transaction()) {
        qWarning() << "deleteProfile transaction failed:" << m_db.lastError().text();
        return false;
    }

    const QStringList statements = {
        QStringLiteral("DELETE FROM facts WHERE persona_key IN "
                       "(SELECT id FROM roles WHERE profile_id = ?)"),
        QStringLiteral("DELETE FROM messages WHERE session_id IN "
                       "(SELECT id FROM sessions WHERE persona_key IN "
                       "(SELECT id FROM roles WHERE profile_id = ?))"),
        QStringLiteral("DELETE FROM sessions WHERE persona_key IN "
                       "(SELECT id FROM roles WHERE profile_id = ?)"),
        QStringLiteral("DELETE FROM roles WHERE profile_id = ?"),
        QStringLiteral("DELETE FROM user_profiles WHERE id = ?"),
    };
    for (const QString &sql : statements) {
        QSqlQuery q(m_db);
        q.prepare(sql);
        q.addBindValue(profileId);
        if (!q.exec()) {
            qWarning() << "deleteProfile statement failed:" << q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        qWarning() << "deleteProfile commit failed:" << m_db.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteRole(const QString &roleId)
{
    if (!m_online || roleId.isEmpty())
        return false;
    if (!m_db.transaction()) {
        qWarning() << "deleteRole transaction failed:" << m_db.lastError().text();
        return false;
    }

    const QStringList statements = {
        QStringLiteral("DELETE FROM facts WHERE persona_key = ?"),
        QStringLiteral("DELETE FROM messages WHERE session_id IN "
                       "(SELECT id FROM sessions WHERE persona_key = ?)"),
        QStringLiteral("DELETE FROM sessions WHERE persona_key = ?"),
        QStringLiteral("DELETE FROM roles WHERE id = ?"),
    };
    for (const QString &sql : statements) {
        QSqlQuery q(m_db);
        q.prepare(sql);
        q.addBindValue(roleId);
        if (!q.exec()) {
            qWarning() << "deleteRole statement failed:" << q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        qWarning() << "deleteRole commit failed:" << m_db.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteSession(const QString &sessionId)
{
    if (!m_online || sessionId.isEmpty())
        return false;
    if (!m_db.transaction()) {
        qWarning() << "deleteSession transaction failed:" << m_db.lastError().text();
        return false;
    }
    QSqlQuery messages(m_db);
    messages.prepare(QStringLiteral("DELETE FROM messages WHERE session_id = ?"));
    messages.addBindValue(sessionId);
    QSqlQuery session(m_db);
    session.prepare(QStringLiteral("DELETE FROM sessions WHERE id = ?"));
    session.addBindValue(sessionId);
    if (!messages.exec() || !session.exec()) {
        qWarning() << "deleteSession failed";
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        qWarning() << "deleteSession commit failed:" << m_db.lastError().text();
        return false;
    }
    return true;
}

QVector<CustomRole> Database::rolesForProfile(const QString &profileId, int limit)
{
    QVector<CustomRole> result;
    if (!m_online || profileId.isEmpty())
        return result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, profile_id, relation, name, role_gender, role_age, personality, "
        "system_prompt, tagline, avatar, created_at "
        "FROM roles WHERE profile_id = ? ORDER BY created_at, rowid LIMIT ?"));
    q.addBindValue(profileId);
    q.addBindValue(qMax(1, limit));
    if (!q.exec())
        return result;
    while (q.next()) {
        CustomRole r;
        r.id = q.value(0).toString();
        r.profileId = q.value(1).toString();
        r.relation = q.value(2).toString();
        r.name = q.value(3).toString();
        r.roleGender = q.value(4).toString();
        r.age = q.value(5).toInt();
        r.personality = q.value(6).toString();
        r.systemPrompt = q.value(7).toString();
        r.tagline = q.value(8).toString();
        r.avatar = q.value(9).toString();
        r.createdAt = parseDbTime(q.value(10).toString());
        if (r.isValid())
            result.append(r);
    }
    return result;
}

bool Database::insertRole(const CustomRole &role)
{
    if (!m_online || !role.isValid())
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO roles (id, profile_id, relation, name, role_gender, role_age, "
        "personality, system_prompt, tagline, avatar) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(role.id);
    q.addBindValue(role.profileId);
    q.addBindValue(role.relation);
    q.addBindValue(role.name);
    q.addBindValue(role.roleGender);
    q.addBindValue(role.age);
    q.addBindValue(role.personality);
    q.addBindValue(role.systemPrompt);
    q.addBindValue(role.tagline);
    q.addBindValue(role.avatar);
    if (!q.exec()) {
        qWarning() << "insertRole failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::setRoleName(const QString &roleId, const QString &name)
{
    if (!m_online || roleId.isEmpty() || name.trimmed().isEmpty())
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE roles SET name = ? WHERE id = ? AND (name IS NULL OR TRIM(name) = '')"));
    q.addBindValue(name.trimmed().left(20));
    q.addBindValue(roleId);
    if (!q.exec()) {
        qWarning() << "setRoleName failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

CustomRole Database::roleById(const QString &roleId)
{
    CustomRole role;
    if (!m_online || roleId.isEmpty())
        return role;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, profile_id, relation, name, role_gender, role_age, personality, "
        "system_prompt, tagline, avatar, created_at "
        "FROM roles WHERE id = ?"));
    q.addBindValue(roleId);
    if (!q.exec() || !q.next())
        return role;
    role.id = q.value(0).toString();
    role.profileId = q.value(1).toString();
    role.relation = q.value(2).toString();
    role.name = q.value(3).toString();
    role.roleGender = q.value(4).toString();
    role.age = q.value(5).toInt();
    role.personality = q.value(6).toString();
    role.systemPrompt = q.value(7).toString();
    role.tagline = q.value(8).toString();
    role.avatar = q.value(9).toString();
    role.createdAt = parseDbTime(q.value(10).toString());
    return role;
}

QVector<SessionInfo> Database::sessionsForRole(const QString &roleId, int limit)
{
    QVector<SessionInfo> result;
    if (!m_online || roleId.isEmpty())
        return result;

    const int visibleLimit = qMax(1, limit);

    // 锁定记录按锁定先后顺序固定在最前，永远不会被最近记录挤出显示列表。
    QSqlQuery locked(m_db);
    locked.prepare(QStringLiteral(
        "SELECT id, created_at, locked FROM sessions "
        "WHERE persona_key = ? AND locked = 1 "
        "ORDER BY locked_at ASC, rowid ASC"));
    locked.addBindValue(roleId);
    if (locked.exec()) {
        while (locked.next()) {
            SessionInfo s;
            s.id = locked.value(0).toString();
            s.createdAt = parseDbTime(locked.value(1).toString());
            s.locked = locked.value(2).toInt() != 0;
            if (!s.id.isEmpty())
                result.append(s);
        }
    }

    const int remaining = visibleLimit - result.size();
    if (remaining <= 0)
        return result;

    QSqlQuery recent(m_db);
    recent.prepare(QStringLiteral(
        "SELECT id, created_at, locked FROM sessions "
        "WHERE persona_key = ? AND locked = 0 "
        "ORDER BY created_at DESC, rowid DESC LIMIT ?"));
    recent.addBindValue(roleId);
    recent.addBindValue(remaining);
    if (recent.exec()) {
        while (recent.next()) {
            SessionInfo s;
            s.id = recent.value(0).toString();
            s.createdAt = parseDbTime(recent.value(1).toString());
            s.locked = recent.value(2).toInt() != 0;
            if (!s.id.isEmpty())
                result.append(s);
        }
    }
    return result;
}

bool Database::setSessionLocked(const QString &sessionId, bool locked)
{
    if (!m_online || sessionId.isEmpty())
        return false;
    QSqlQuery q(m_db);
    if (locked) {
        q.prepare(QStringLiteral(
            "UPDATE sessions SET locked = 1, locked_at = ? WHERE id = ? AND locked = 0"));
        q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE sessions SET locked = 0, locked_at = NULL WHERE id = ?"));
    }
    q.addBindValue(sessionId);
    if (!q.exec()) {
        qWarning() << "setSessionLocked failed:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

QVector<ChatTurn> Database::messagesForSession(const QString &sessionId)
{
    QVector<ChatTurn> result;
    if (!m_online || sessionId.isEmpty())
        return result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT sender, content, created_at FROM messages WHERE session_id = ? ORDER BY id"));
    q.addBindValue(sessionId);
    if (!q.exec())
        return result;
    while (q.next()) {
        ChatTurn t;
        t.role = q.value(0).toString();
        t.content = q.value(1).toString();
        t.at = parseDbTime(q.value(2).toString());
        result.append(t);
    }
    return result;
}

bool Database::insertSessionNow(const QString &id, const QString &gender, const QString &roleKey,
                                const QString &personaKey, const QString &startedAt)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sessions (id, user_gender, role_key, persona_key, created_at) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET user_gender = excluded.user_gender, "
        "role_key = excluded.role_key, persona_key = excluded.persona_key"));
    q.addBindValue(id);
    q.addBindValue(gender);
    q.addBindValue(roleKey);
    q.addBindValue(personaKey);
    q.addBindValue(startedAt.isEmpty()
                       ? QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                       : startedAt);
    if (!q.exec()) {
        qWarning() << "saveSession failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::insertMessageNow(const QString &sessionId, const QString &sender, const QString &content)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO messages (session_id, sender, content) VALUES (?, ?, ?)"));
    q.addBindValue(sessionId);
    q.addBindValue(sender);
    q.addBindValue(content);
    if (!q.exec()) {
        qWarning() << "saveMessage failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::saveSession(const QString &id, const QString &gender, const QString &roleKey,
                           const QString &personaKey, const QString &startedAt)
{
    if (m_online && insertSessionNow(id, gender, roleKey, personaKey, startedAt))
        return true;
    PendingSession s;
    s.id = id;
    s.gender = gender;
    s.roleKey = roleKey;
    s.personaKey = personaKey;
    s.startedAt = startedAt;
    queueSession(s);
    return false;
}

bool Database::saveMessage(const QString &sessionId, const QString &sender, const QString &content)
{
    if (m_online && insertMessageNow(sessionId, sender, content))
        return true;
    PendingMessage m;
    m.sessionId = sessionId;
    m.sender = sender;
    m.content = content;
    queueMessage(m);
    return false;
}

bool Database::storeFacts(const QString &personaKey, const QString &userText)
{
    if (!m_online || personaKey.isEmpty() || userText.trimmed().isEmpty())
        return false;

    const QStringList found = extractFacts(userText);
    if (found.isEmpty())
        return true;

    for (const QString &fact : found) {
        QSqlQuery exists(m_db);
        exists.prepare(QStringLiteral("SELECT 1 FROM facts WHERE persona_key = ? AND fact = ? LIMIT 1"));
        exists.addBindValue(personaKey);
        exists.addBindValue(fact);
        if (exists.exec() && exists.next())
            continue;

        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral("INSERT INTO facts (persona_key, fact) VALUES (?, ?)"));
        insert.addBindValue(personaKey);
        insert.addBindValue(fact);
        if (!insert.exec())
            qWarning() << "storeFact failed:" << insert.lastError().text();
    }

    // 每个角色最多保留 40 条，避免 system prompt 无限膨胀。
    QSqlQuery prune(m_db);
    prune.prepare(QStringLiteral(
        "DELETE FROM facts WHERE persona_key = ? AND id NOT IN "
        "(SELECT id FROM facts WHERE persona_key = ? ORDER BY id DESC LIMIT 40)"));
    prune.addBindValue(personaKey);
    prune.addBindValue(personaKey);
    prune.exec();
    return true;
}

QStringList Database::facts(const QString &personaKey, int limit) const
{
    QStringList result;
    if (!m_online || personaKey.isEmpty())
        return result;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT fact FROM facts WHERE persona_key = ? ORDER BY id DESC LIMIT ?"));
    q.addBindValue(personaKey);
    q.addBindValue(qMax(1, limit));
    if (!q.exec())
        return result;
    while (q.next())
        result.prepend(q.value(0).toString());
    return result;
}

bool Database::flushPending()
{
    if (!m_online || pendingCount() == 0)
        return true;
    if (!m_db.transaction()) {
        qWarning() << "pending transaction failed:" << m_db.lastError().text();
        return false;
    }

    int written = 0;
    QVector<PendingSession> sessionsLeft;
    for (const PendingSession &s : std::as_const(m_pendingSessions)) {
        if (insertSessionNow(s.id, s.gender, s.roleKey, s.personaKey, s.startedAt)) {
            ++written;
        } else {
            sessionsLeft.append(s);
        }
    }
    m_pendingSessions = sessionsLeft;

    QVector<PendingMessage> messagesLeft;
    for (const PendingMessage &m : std::as_const(m_pendingMessages)) {
        if (insertMessageNow(m.sessionId, m.sender, m.content)) {
            ++written;
        } else {
            messagesLeft.append(m);
        }
    }
    m_pendingMessages = messagesLeft;

    if (!m_db.commit()) {
        qWarning() << "pending commit failed:" << m_db.lastError().text();
        return false;
    }

    savePending();
    if (written > 0) {
        qInfo() << "Pending records flushed:" << written;
        emit pendingFlushed(written);
    }
    return pendingCount() == 0;
}

void Database::queueSession(const PendingSession &s)
{
    for (const PendingSession &existing : std::as_const(m_pendingSessions)) {
        if (existing.id == s.id)
            return;
    }
    m_pendingSessions.append(s);
    savePending();
}

void Database::queueMessage(const PendingMessage &m)
{
    if (m_pendingMessages.size() >= 5000)
        m_pendingMessages.removeFirst();
    m_pendingMessages.append(m);
    savePending();
}

void Database::loadPending()
{
    m_pendingSessions.clear();
    m_pendingMessages.clear();

    QFile f(AppConfig::instance().pendingFile());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();

    for (const QJsonValue &v : root.value(QStringLiteral("sessions")).toArray()) {
        const QJsonObject o = v.toObject();
        PendingSession s;
        s.id = o.value(QStringLiteral("id")).toString();
        s.gender = o.value(QStringLiteral("gender")).toString();
        s.roleKey = o.value(QStringLiteral("role")).toString();
        s.personaKey = o.value(QStringLiteral("persona")).toString();
        s.startedAt = o.value(QStringLiteral("started_at")).toString();
        if (!s.id.isEmpty())
            m_pendingSessions.append(s);
    }

    for (const QJsonValue &v : root.value(QStringLiteral("messages")).toArray()) {
        const QJsonObject o = v.toObject();
        PendingMessage m;
        m.sessionId = o.value(QStringLiteral("session")).toString();
        m.sender = o.value(QStringLiteral("sender")).toString();
        m.content = o.value(QStringLiteral("content")).toString();
        if (!m.sessionId.isEmpty())
            m_pendingMessages.append(m);
    }
    if (pendingCount() > 0)
        qInfo() << "Loaded pending records:" << pendingCount();
}

bool Database::savePending()
{
    if (pendingCount() == 0) {
        QFile::remove(AppConfig::instance().pendingFile());
        return true;
    }

    QJsonArray sessions;
    for (const PendingSession &s : std::as_const(m_pendingSessions)) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), s.id);
        o.insert(QStringLiteral("gender"), s.gender);
        o.insert(QStringLiteral("role"), s.roleKey);
        o.insert(QStringLiteral("persona"), s.personaKey);
        o.insert(QStringLiteral("started_at"), s.startedAt);
        sessions.append(o);
    }

    QJsonArray messages;
    for (const PendingMessage &m : std::as_const(m_pendingMessages)) {
        QJsonObject o;
        o.insert(QStringLiteral("session"), m.sessionId);
        o.insert(QStringLiteral("sender"), m.sender);
        o.insert(QStringLiteral("content"), m.content);
        messages.append(o);
    }

    QJsonObject root;
    root.insert(QStringLiteral("sessions"), sessions);
    root.insert(QStringLiteral("messages"), messages);

    QSaveFile f(AppConfig::instance().pendingFile());
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

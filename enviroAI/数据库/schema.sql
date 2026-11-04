-- 心屿 · AI 情境对话 —— SQLite 本地数据库结构
-- 默认位置：data/xinyu.db

PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS user_profiles (
  id         TEXT PRIMARY KEY,
  gender     TEXT NOT NULL,
  name       TEXT NOT NULL DEFAULT '',
  age        INTEGER NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS roles (
  id           TEXT PRIMARY KEY,
  profile_id   TEXT NOT NULL,
  relation     TEXT NOT NULL,
  name         TEXT NOT NULL DEFAULT '',
  role_gender  TEXT NOT NULL,
  role_age     INTEGER NOT NULL,
  personality  TEXT NOT NULL,
  system_prompt TEXT NOT NULL,
  tagline      TEXT NOT NULL DEFAULT '',
  avatar       TEXT NOT NULL DEFAULT '',
  created_at   TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_roles_profile
  ON roles (profile_id, created_at);

CREATE TABLE IF NOT EXISTS sessions (
  id          TEXT PRIMARY KEY,
  user_gender TEXT NOT NULL,
  role_key    TEXT NOT NULL,
  persona_key TEXT NOT NULL,
  locked      INTEGER NOT NULL DEFAULT 0,
  locked_at   TEXT,
  created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_sessions_created
  ON sessions (created_at);
CREATE INDEX IF NOT EXISTS idx_sessions_persona
  ON sessions (persona_key);

CREATE TABLE IF NOT EXISTS messages (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  session_id TEXT NOT NULL,
  sender     TEXT NOT NULL,
  content    TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_messages_session
  ON messages (session_id, id);

CREATE TABLE IF NOT EXISTS facts (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  persona_key TEXT NOT NULL,
  fact        TEXT NOT NULL,
  created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_facts_persona
  ON facts (persona_key, id);

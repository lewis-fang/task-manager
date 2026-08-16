-- KSAT schema (SQLite)
-- The application creates this automatically on first run
-- (see Database::ensureSchema). This file documents the schema and can be
-- used to inspect / recreate a database with the sqlite3 CLI:
--   sqlite3 ksat.db < schema.sql

CREATE TABLE IF NOT EXISTS projects (
    id    INTEGER PRIMARY KEY AUTOINCREMENT,
    name  TEXT NOT NULL UNIQUE,
    color TEXT NOT NULL DEFAULT '#FFFFFF'
);

CREATE TABLE IF NOT EXISTS people (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

-- status: not_started / in_progress / completed / delayed / stopped
-- main_tasks and sub_tasks reference each other (source_subtask_id); SQLite
-- allows the circular FK at CREATE time and enforces it only on DML.
CREATE TABLE IF NOT EXISTS main_tasks (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    name              TEXT NOT NULL,
    project_id        INTEGER NULL,
    people_id         INTEGER NULL,
    start_time        TEXT NULL,
    end_time          TEXT NULL,
    status            TEXT NOT NULL DEFAULT 'not_started',
    status_set_time   TEXT NULL,          -- set when status completed/stopped is set manually
    source_subtask_id INTEGER NULL,       -- the subtask this main task was created from
    sort_order        INTEGER NOT NULL DEFAULT 0, -- display order within the same project (drag-to-sort)
    created_at        TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE SET NULL,
    FOREIGN KEY (people_id)  REFERENCES people(id)  ON DELETE SET NULL,
    FOREIGN KEY (source_subtask_id) REFERENCES sub_tasks(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS sub_tasks (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    main_task_id    INTEGER NOT NULL,
    name            TEXT NOT NULL,
    start_time      TEXT NULL,
    end_time        TEXT NULL,
    status          TEXT NOT NULL DEFAULT 'not_started',
    status_set_time TEXT NULL,
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (main_task_id) REFERENCES main_tasks(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS records (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    main_task_id INTEGER NULL,
    sub_task_id  INTEGER NULL,
    content      TEXT NOT NULL,
    created_at   TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (main_task_id) REFERENCES main_tasks(id) ON DELETE CASCADE,
    FOREIGN KEY (sub_task_id)  REFERENCES sub_tasks(id)  ON DELETE CASCADE
);
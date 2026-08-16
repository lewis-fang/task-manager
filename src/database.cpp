#include "database.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <QDebug>

namespace ksat {

static const char *kConnection = "ksat_main";

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static QString qstr(const QVariant &v) { return v.isNull() ? QString() : v.toString(); }

static QDateTime qdt(const QVariant &v)
{
    return v.isNull() ? QDateTime() : v.toDateTime();
}

static int qint(const QVariant &v) { return v.isNull() ? -1 : v.toInt(); }

static TaskStatus qstatus(const QVariant &v)
{
    return statusFromKey(qstr(v));
}

static QVariant toDateTimeQVariant(const QDateTime &dt)
{
    return dt.isValid() ? QVariant(dt.toString(Qt::ISODate)) : QVariant(QVariant::String);
}

// ---------------------------------------------------------------------------
// status helpers
// ---------------------------------------------------------------------------

QString statusKey(TaskStatus s)
{
    switch (s) {
    case TaskStatus::NotStarted: return "not_started";
    case TaskStatus::InProgress: return "in_progress";
    case TaskStatus::Completed:  return "completed";
    case TaskStatus::Delayed:    return "delayed";
    case TaskStatus::Stopped:    return "stopped";
    }
    return "not_started";
}

TaskStatus statusFromKey(const QString &k)
{
    if (k == "in_progress") return TaskStatus::InProgress;
    if (k == "completed")   return TaskStatus::Completed;
    if (k == "delayed")     return TaskStatus::Delayed;
    if (k == "stopped")     return TaskStatus::Stopped;
    return TaskStatus::NotStarted;
}

TaskStatus MainTask::displayStatus() const
{
    if (manualStatus == TaskStatus::Completed || manualStatus == TaskStatus::Stopped)
        return manualStatus;
    return Database::computeAutoStatus(startTime, endTime);
}

TaskStatus SubTask::displayStatus() const
{
    if (manualStatus == TaskStatus::Completed || manualStatus == TaskStatus::Stopped)
        return manualStatus;
    return Database::computeAutoStatus(startTime, endTime);
}

// ---------------------------------------------------------------------------
// connection
// ---------------------------------------------------------------------------

QSqlDatabase Database::db()
{
    return QSqlDatabase::database(kConnection);
}

QString Database::connect(const QString &dbFilePath)
{
    QSqlDatabase d = QSqlDatabase::addDatabase("QSQLITE", kConnection);
    d.setDatabaseName(dbFilePath);

    if (!d.open()) {
        QString err = d.lastError().text();
        QSqlDatabase::removeDatabase(kConnection);
        return err.isEmpty() ? QStringLiteral("unknown error") : err;
    }

    // Enforce FK constraints (ON DELETE CASCADE / SET NULL) like MySQL did.
    // SQLite keeps them disabled by default.
    QSqlQuery q(d);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    return QString();
}

bool Database::isConnected()
{
    return QSqlDatabase::contains(kConnection) && db().isOpen();
}

void Database::disconnect()
{
    if (QSqlDatabase::contains(kConnection)) {
        db().close();
        QSqlDatabase::removeDatabase(kConnection);
    }
}

// ---------------------------------------------------------------------------
// schema
// ---------------------------------------------------------------------------

QString Database::ensureSchema()
{
    static const char *statements[] = {
        // SQLite: each statement is executed separately (single statements).
        "CREATE TABLE IF NOT EXISTS projects ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  color TEXT NOT NULL DEFAULT '#FFFFFF'"
        ")",

        "CREATE TABLE IF NOT EXISTS people ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE"
        ")",

        // Note: main_tasks references sub_tasks (source_subtask_id) and vice
        // versa; SQLite allows the circular FK at CREATE time and enforces it
        // only on DML, so table order does not matter.
        "CREATE TABLE IF NOT EXISTS main_tasks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  project_id INTEGER NULL,"
        "  people_id INTEGER NULL,"
        "  start_time TEXT NULL,"
        "  end_time TEXT NULL,"
        "  status TEXT NOT NULL DEFAULT 'not_started',"
        "  status_set_time TEXT NULL,"
        "  source_subtask_id INTEGER NULL,"
        "  sort_order INTEGER NOT NULL DEFAULT 0,"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "  FOREIGN KEY (project_id) REFERENCES projects(id) ON DELETE SET NULL,"
        "  FOREIGN KEY (people_id) REFERENCES people(id) ON DELETE SET NULL,"
        "  FOREIGN KEY (source_subtask_id) REFERENCES sub_tasks(id) ON DELETE SET NULL"
        ")",

        // Upgrade path for databases created before sort_order existed.
        "ALTER TABLE main_tasks ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0",

        "CREATE TABLE IF NOT EXISTS sub_tasks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  main_task_id INTEGER NOT NULL,"
        "  name TEXT NOT NULL,"
        "  start_time TEXT NULL,"
        "  end_time TEXT NULL,"
        "  status TEXT NOT NULL DEFAULT 'not_started',"
        "  status_set_time TEXT NULL,"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "  FOREIGN KEY (main_task_id) REFERENCES main_tasks(id) ON DELETE CASCADE"
        ")",

        "CREATE TABLE IF NOT EXISTS records ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  main_task_id INTEGER NULL,"
        "  sub_task_id INTEGER NULL,"
        "  content TEXT NOT NULL,"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "  FOREIGN KEY (main_task_id) REFERENCES main_tasks(id) ON DELETE CASCADE,"
        "  FOREIGN KEY (sub_task_id) REFERENCES sub_tasks(id) ON DELETE CASCADE"
        ")",
    };

    for (const char *sql : statements) {
        QSqlQuery q(db());
        if (!q.exec(QString::fromLatin1(sql))) {
            const QString err = q.lastError().text();
            // "Duplicate column name" means sort_order is already present
            // (upgrade path on an already-migrated database).
            if (err.contains(QStringLiteral("Duplicate column"), Qt::CaseInsensitive))
                continue;
            return err;
        }
    }
    return QString();
}

// ---------------------------------------------------------------------------
// projects / people
// ---------------------------------------------------------------------------

QColor projectColor(int id)
{
    // Deterministic color from a small curated palette.
    static const QColor palette[] = {
        QColor("#E53935"), QColor("#8E24AA"), QColor("#3949AB"), QColor("#00897B"),
        QColor("#7CB342"), QColor("#F4511E"), QColor("#C0CA33"), QColor("#D81B60"),
        QColor("#5E35B1"), QColor("#00ACC1"), QColor("#FB8C00"), QColor("#43A047")
    };
    const int n = int(sizeof(palette) / sizeof(palette[0]));
    return palette[((id % n) + n) % n];
}

int Database::ensureProject(const QString &name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return -1;

    QSqlQuery q(db());
    q.prepare("SELECT id FROM projects WHERE name = ?");
    q.addBindValue(trimmed);
    if (q.exec() && q.next())
        return q.value(0).toInt();

    q.prepare("INSERT INTO projects (name, color) VALUES (?, ?)");
    q.addBindValue(trimmed);
    q.addBindValue(QStringLiteral("#FFFFFF")); // placeholder; fixed below via id
    if (!q.exec()) return -1;

    int id = q.lastInsertId().toInt();
    // assign color now that we know the id
    QSqlQuery u(db());
    u.prepare("UPDATE projects SET color = ? WHERE id = ?");
    u.addBindValue(projectColor(id).name());
    u.addBindValue(id);
    u.exec();
    return id;
}

int Database::ensurePerson(const QString &name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return -1;

    QSqlQuery q(db());
    q.prepare("SELECT id FROM people WHERE name = ?");
    q.addBindValue(trimmed);
    if (q.exec() && q.next())
        return q.value(0).toInt();

    q.prepare("INSERT INTO people (name) VALUES (?)");
    q.addBindValue(trimmed);
    return q.exec() ? q.lastInsertId().toInt() : -1;
}

QVector<Project> Database::loadProjects()
{
    QVector<Project> out;
    QSqlQuery q(db());
    q.exec(QStringLiteral("SELECT id, name, color FROM projects ORDER BY name"));
    while (q.next()) {
        Project p;
        p.id = q.value(0).toInt();
        p.name = q.value(1).toString();
        p.color = QColor(q.value(2).toString());
        out.append(p);
    }
    return out;
}

QVector<Person> Database::loadPeople()
{
    QVector<Person> out;
    QSqlQuery q(db());
    q.exec(QStringLiteral("SELECT id, name FROM people ORDER BY name"));
    while (q.next()) {
        Person p;
        p.id = q.value(0).toInt();
        p.name = q.value(1).toString();
        out.append(p);
    }
    return out;
}

QString Database::projectName(int id)
{
    if (id < 0) return QString();
    QSqlQuery q(db());
    q.prepare("SELECT name FROM projects WHERE id = ?");
    q.addBindValue(id);
    return (q.exec() && q.next()) ? q.value(0).toString() : QString();
}

QString Database::personName(int id)
{
    if (id < 0) return QString();
    QSqlQuery q(db());
    q.prepare("SELECT name FROM people WHERE id = ?");
    q.addBindValue(id);
    return (q.exec() && q.next()) ? q.value(0).toString() : QString();
}

// ---------------------------------------------------------------------------
// main tasks
// ---------------------------------------------------------------------------

static MainTask readMainTask(QSqlQuery &q)
{
    MainTask t;
    t.id = q.value(0).toInt();
    t.name = q.value(1).toString();
    t.projectId = qint(q.value(2));
    t.peopleId = qint(q.value(3));
    t.startTime = qdt(q.value(4));
    t.endTime = qdt(q.value(5));
    t.manualStatus = qstatus(q.value(6));
    t.statusSetTime = qdt(q.value(7));
    t.sourceSubtaskId = qint(q.value(8));
    t.sortOrder = q.value(9).toInt();
    t.createdAt = qdt(q.value(10));
    return t;
}

QVector<MainTask> Database::loadMainTasks(const QVector<int> &projectIds,
                                          const QVector<int> &peopleIds,
                                          const QVector<TaskStatus> &statuses,
                                          bool includeFinished)
{
    QVector<MainTask> out;
    QStringList cond;
    QVariantList args;

    if (!projectIds.isEmpty()) {
        QStringList ps;
        for (int p : projectIds) { ps << QStringLiteral("?"); args << p; }
        cond << QStringLiteral("project_id IN (%1)").arg(ps.join(QStringLiteral(",")));
    }
    if (!peopleIds.isEmpty()) {
        QStringList ps;
        for (int p : peopleIds) { ps << QStringLiteral("?"); args << p; }
        cond << QStringLiteral("people_id IN (%1)").arg(ps.join(QStringLiteral(",")));
    }

    if (!includeFinished) {
        cond << "status NOT IN ('completed','stopped')";
    } else if (!statuses.isEmpty()) {
        QStringList ks;
        for (TaskStatus s : statuses) ks << QStringLiteral("'%1'").arg(statusKey(s));
        cond << QStringLiteral("status IN (%1)").arg(ks.join(QStringLiteral(",")));
    }

    QString sql = QStringLiteral(
        "SELECT id, name, project_id, people_id, start_time, end_time, status,"
        " status_set_time, source_subtask_id, sort_order, created_at FROM main_tasks");
    if (!cond.isEmpty()) sql += QStringLiteral(" WHERE ") + cond.join(QStringLiteral(" AND "));
    // Same-project tasks sit next to each other (NULL project forms its own group
    // at the front); within a project the drag-sort order wins, creation order
    // breaks ties for tasks that were never manually reordered.
    sql += QStringLiteral(" ORDER BY COALESCE(project_id, 0), sort_order, created_at, id");

    QSqlQuery q(db());
    q.prepare(sql);
    for (const QVariant &a : args) q.addBindValue(a);
    if (!q.exec()) { qWarning() << "loadMainTasks:" << q.lastError().text(); return out; }
    while (q.next()) out.append(readMainTask(q));
    return out;
}

MainTask Database::mainTask(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, name, project_id, people_id, start_time, end_time, status,"
        " status_set_time, source_subtask_id, sort_order, created_at FROM main_tasks WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) return readMainTask(q);
    return MainTask();
}

int Database::insertMainTask(const MainTask &t)
{
    // Append to the end of the same project group.
    QSqlQuery qr(db());
    qr.prepare(QStringLiteral(
        "SELECT COALESCE(MAX(sort_order), -1) + 1 FROM main_tasks"
        " WHERE COALESCE(project_id, 0) = ?"));
    qr.addBindValue(t.projectId < 0 ? 0 : t.projectId);
    int order = 0;
    if (qr.exec() && qr.next()) order = qr.value(0).toInt();

    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO main_tasks (name, project_id, people_id, start_time, end_time,"
        " status, status_set_time, source_subtask_id, sort_order, created_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(t.name);
    q.addBindValue(t.projectId < 0 ? QVariant(QVariant::Int) : QVariant(t.projectId));
    q.addBindValue(t.peopleId < 0 ? QVariant(QVariant::Int) : QVariant(t.peopleId));
    q.addBindValue(toDateTimeQVariant(t.startTime));
    q.addBindValue(toDateTimeQVariant(t.endTime));
    q.addBindValue(statusKey(t.manualStatus));
    q.addBindValue(toDateTimeQVariant(t.statusSetTime));
    q.addBindValue(t.sourceSubtaskId < 0 ? QVariant(QVariant::Int) : QVariant(t.sourceSubtaskId));
    q.addBindValue(order);
    q.addBindValue(toDateTimeQVariant(t.createdAt.isValid() ? t.createdAt : QDateTime::currentDateTime()));
    if (!q.exec()) { qWarning() << "insertMainTask:" << q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

bool Database::updateMainTask(const MainTask &t)
{
    // When the task moves to a different project, append it to the end of the
    // new project group so the "same project tasks together" invariant holds.
    const MainTask old = mainTask(t.id);
    const bool projectChanged = old.id >= 0 && old.projectId != t.projectId;

    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE main_tasks SET name = ?, project_id = ?, people_id = ?, start_time = ?,"
        " end_time = ?, status = ?, status_set_time = ?, source_subtask_id = ? WHERE id = ?"));
    q.addBindValue(t.name);
    q.addBindValue(t.projectId < 0 ? QVariant(QVariant::Int) : QVariant(t.projectId));
    q.addBindValue(t.peopleId < 0 ? QVariant(QVariant::Int) : QVariant(t.peopleId));
    q.addBindValue(toDateTimeQVariant(t.startTime));
    q.addBindValue(toDateTimeQVariant(t.endTime));
    q.addBindValue(statusKey(t.manualStatus));
    q.addBindValue(toDateTimeQVariant(t.statusSetTime));
    q.addBindValue(t.sourceSubtaskId < 0 ? QVariant(QVariant::Int) : QVariant(t.sourceSubtaskId));
    q.addBindValue(t.id);
    if (!q.exec()) { qWarning() << "updateMainTask:" << q.lastError().text(); return false; }

    if (projectChanged) {
        QSqlQuery qr(db());
        qr.prepare(QStringLiteral(
            "SELECT COALESCE(MAX(sort_order), -1) + 1 FROM main_tasks"
            " WHERE COALESCE(project_id, 0) = ?"));
        qr.addBindValue(t.projectId < 0 ? 0 : t.projectId);
        int order = 0;
        if (qr.exec() && qr.next()) order = qr.value(0).toInt();
        QSqlQuery u(db());
        u.prepare(QStringLiteral("UPDATE main_tasks SET sort_order = ? WHERE id = ?"));
        u.addBindValue(order);
        u.addBindValue(t.id);
        if (!u.exec()) { qWarning() << "updateMainTask reorder:" << u.lastError().text(); return false; }
    }
    return true;
}

bool Database::deleteMainTask(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM main_tasks WHERE id = ?"));
    q.addBindValue(id);
    return q.exec();
}

bool Database::reorderMainTasks(int projectId, const QVector<int> &orderedIds)
{
    QSqlDatabase d = db();
    if (!d.transaction()) return false;

    const int groupKey = projectId < 0 ? 0 : projectId;
    for (int i = 0; i < orderedIds.size(); ++i) {
        QSqlQuery q(d);
        q.prepare(QStringLiteral(
            "UPDATE main_tasks SET sort_order = ? WHERE id = ? AND COALESCE(project_id, 0) = ?"));
        q.addBindValue(i);
        q.addBindValue(orderedIds.at(i));
        q.addBindValue(groupKey);
        if (!q.exec()) {
            qWarning() << "reorderMainTasks:" << q.lastError().text();
            d.rollback();
            return false;
        }
    }
    return d.commit();
}

// ---------------------------------------------------------------------------
// sub tasks
// ---------------------------------------------------------------------------

static SubTask readSubTask(QSqlQuery &q)
{
    SubTask s;
    s.id = q.value(0).toInt();
    s.mainTaskId = q.value(1).toInt();
    s.name = q.value(2).toString();
    s.startTime = qdt(q.value(3));
    s.endTime = qdt(q.value(4));
    s.manualStatus = qstatus(q.value(5));
    s.statusSetTime = qdt(q.value(6));
    s.createdAt = qdt(q.value(7));
    return s;
}

QVector<SubTask> Database::loadSubTasks(int mainTaskId)
{
    QVector<SubTask> out;
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, main_task_id, name, start_time, end_time, status, status_set_time, created_at"
        " FROM sub_tasks WHERE main_task_id = ? ORDER BY created_at, id"));
    q.addBindValue(mainTaskId);
    if (!q.exec()) { qWarning() << "loadSubTasks:" << q.lastError().text(); return out; }
    while (q.next()) out.append(readSubTask(q));
    return out;
}

int Database::insertSubTask(const SubTask &s)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO sub_tasks (main_task_id, name, start_time, end_time, status,"
        " status_set_time, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(s.mainTaskId);
    q.addBindValue(s.name);
    q.addBindValue(toDateTimeQVariant(s.startTime));
    q.addBindValue(toDateTimeQVariant(s.endTime));
    q.addBindValue(statusKey(s.manualStatus));
    q.addBindValue(toDateTimeQVariant(s.statusSetTime));
    q.addBindValue(toDateTimeQVariant(s.createdAt.isValid() ? s.createdAt : QDateTime::currentDateTime()));
    if (!q.exec()) { qWarning() << "insertSubTask:" << q.lastError().text(); return -1; }
    return q.lastInsertId().toInt();
}

bool Database::updateSubTask(const SubTask &s)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE sub_tasks SET name = ?, start_time = ?, end_time = ?, status = ?,"
        " status_set_time = ? WHERE id = ?"));
    q.addBindValue(s.name);
    q.addBindValue(toDateTimeQVariant(s.startTime));
    q.addBindValue(toDateTimeQVariant(s.endTime));
    q.addBindValue(statusKey(s.manualStatus));
    q.addBindValue(toDateTimeQVariant(s.statusSetTime));
    q.addBindValue(s.id);
    if (!q.exec()) { qWarning() << "updateSubTask:" << q.lastError().text(); return false; }
    return true;
}

bool Database::deleteSubTask(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM sub_tasks WHERE id = ?"));
    q.addBindValue(id);
    return q.exec();
}

SubTask Database::subTask(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, main_task_id, name, start_time, end_time, status, status_set_time, created_at"
        " FROM sub_tasks WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) return readSubTask(q);
    return SubTask();
}

QVector<MainTask> Database::mainTasksFromSubtask(int subTaskId)
{
    QVector<MainTask> out;
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, name, project_id, people_id, start_time, end_time, status,"
        " status_set_time, source_subtask_id, sort_order, created_at FROM main_tasks"
        " WHERE source_subtask_id = ? ORDER BY created_at, id"));
    q.addBindValue(subTaskId);
    if (!q.exec()) { qWarning() << "mainTasksFromSubtask:" << q.lastError().text(); return out; }
    while (q.next()) out.append(readMainTask(q));
    return out;
}

// ---------------------------------------------------------------------------
// records
// ---------------------------------------------------------------------------

QVector<Record> Database::loadRecordsForMainTask(int mainTaskId)
{
    QVector<Record> out;
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, main_task_id, sub_task_id, content, created_at FROM records"
        " WHERE main_task_id = ? ORDER BY created_at, id"));
    q.addBindValue(mainTaskId);
    if (!q.exec()) { qWarning() << "loadRecordsForMainTask:" << q.lastError().text(); return out; }
    while (q.next()) {
        Record r;
        r.id = q.value(0).toInt();
        r.mainTaskId = q.value(1).toInt();
        r.subTaskId = qint(q.value(2));
        r.content = q.value(3).toString();
        r.createdAt = qdt(q.value(4));
        out.append(r);
    }
    return out;
}

QVector<Record> Database::loadRecordsForSubTask(int subTaskId)
{
    QVector<Record> out;
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, main_task_id, sub_task_id, content, created_at FROM records"
        " WHERE sub_task_id = ? ORDER BY created_at, id"));
    q.addBindValue(subTaskId);
    if (!q.exec()) { qWarning() << "loadRecordsForSubTask:" << q.lastError().text(); return out; }
    while (q.next()) {
        Record r;
        r.id = q.value(0).toInt();
        r.mainTaskId = qint(q.value(1));
        r.subTaskId = q.value(2).toInt();
        r.content = q.value(3).toString();
        r.createdAt = qdt(q.value(4));
        out.append(r);
    }
    return out;
}

bool Database::addRecord(int mainTaskId, int subTaskId, const QString &content)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO records (main_task_id, sub_task_id, content, created_at)"
        " VALUES (?, ?, ?, ?)"));
    q.addBindValue(mainTaskId < 0 ? QVariant(QVariant::Int) : QVariant(mainTaskId));
    q.addBindValue(subTaskId < 0 ? QVariant(QVariant::Int) : QVariant(subTaskId));
    q.addBindValue(content);
    q.addBindValue(toDateTimeQVariant(QDateTime::currentDateTime()));
    if (!q.exec()) { qWarning() << "addRecord:" << q.lastError().text(); return false; }
    return true;
}

bool Database::updateRecord(int recordId, const QString &content)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE records SET content = ? WHERE id = ?"));
    q.addBindValue(content);
    q.addBindValue(recordId);
    if (!q.exec()) { qWarning() << "updateRecord:" << q.lastError().text(); return false; }
    return true;
}

bool Database::deleteRecord(int recordId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM records WHERE id = ?"));
    q.addBindValue(recordId);
    return q.exec();
}

// ---------------------------------------------------------------------------
// auto status
// ---------------------------------------------------------------------------

TaskStatus Database::computeAutoStatus(const QDateTime &start, const QDateTime &end)
{
    const QDateTime now = QDateTime::currentDateTime();
    if (!start.isValid())
        return TaskStatus::NotStarted;      // no schedule yet -> not started
    if (now < start)
        return TaskStatus::NotStarted;      // scheduled in the future
    if (!end.isValid() || now <= end)
        return TaskStatus::InProgress;      // within the window
    return TaskStatus::Delayed;             // past end time
}

int Database::syncAutoStatuses()
{
    int updated = 0;
    QSqlQuery q(db());
    q.exec(QStringLiteral(
        "SELECT id, start_time, end_time, status FROM main_tasks"
        " WHERE status NOT IN ('completed','stopped')"));
    QVector<std::pair<int, TaskStatus>> pending;
    while (q.next()) {
        const int id = q.value(0).toInt();
        const QDateTime start = qdt(q.value(1));
        const QDateTime end = qdt(q.value(2));
        const TaskStatus cur = qstatus(q.value(3));
        const TaskStatus next = computeAutoStatus(start, end);
        if (next != cur) pending.append({id, next});
    }
    for (const auto &p : pending) {
        QSqlQuery u(db());
        u.prepare(QStringLiteral("UPDATE main_tasks SET status = ? WHERE id = ?"));
        u.addBindValue(statusKey(p.second));
        u.addBindValue(p.first);
        if (u.exec()) ++updated;
    }

    QSqlQuery qs(db());
    qs.exec(QStringLiteral(
        "SELECT id, start_time, end_time, status FROM sub_tasks"
        " WHERE status NOT IN ('completed','stopped')"));
    QVector<std::pair<int, TaskStatus>> spending;
    while (qs.next()) {
        const int id = qs.value(0).toInt();
        const QDateTime start = qdt(qs.value(1));
        const QDateTime end = qdt(qs.value(2));
        const TaskStatus cur = qstatus(qs.value(3));
        const TaskStatus next = computeAutoStatus(start, end);
        if (next != cur) spending.append({id, next});
    }
    for (const auto &p : spending) {
        QSqlQuery u(db());
        u.prepare(QStringLiteral("UPDATE sub_tasks SET status = ? WHERE id = ?"));
        u.addBindValue(statusKey(p.second));
        u.addBindValue(p.first);
        if (u.exec()) ++updated;
    }
    return updated;
}

} // namespace ksat

#pragma once

#include "models.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace ksat {

// Central SQLite access point. Singleton; all methods assume a live connection.
class Database {
public:
    // ---- connection --------------------------------------------------------
    // Open the SQLite database at the given file path (created if missing).
    // Returns empty string on success, otherwise a human-readable error.
    static QString connect(const QString &dbFilePath);

    static bool isConnected();
    static void disconnect();

    // Access the underlying connection (for ad-hoc queries).
    static QSqlDatabase db();

    // Create missing tables (idempotent). Returns error string or empty.
    static QString ensureSchema();

    // ---- projects / people -------------------------------------------------
    // Get-or-create by name (project color auto-assigned deterministically).
    static int ensureProject(const QString &name);
    static int ensurePerson(const QString &name);
    static QVector<Project> loadProjects();
    static QVector<Person> loadPeople();
    static QString projectName(int id);
    static QString personName(int id);

    // ---- main tasks --------------------------------------------------------
    // projectIds/peopleIds: empty = all; otherwise IN-list.
    // statuses: empty = all; otherwise IN-list.
    // includeFinished controls whether completed/stopped tasks are returned.
    static QVector<MainTask> loadMainTasks(const QVector<int> &projectIds,
                                           const QVector<int> &peopleIds,
                                           const QVector<TaskStatus> &statuses,
                                           bool includeFinished);
    static int insertMainTask(const MainTask &t);
    static bool updateMainTask(const MainTask &t);
    static bool deleteMainTask(int id);
    static MainTask mainTask(int id);

    // Persist a new display order for the tasks of one project (drag-to-sort).
    // orderedIds must contain exactly the ids of the tasks belonging to the
    // project; each entry's sort_order is set to its position. projectId < 0
    // addresses the "(no project)" group.
    static bool reorderMainTasks(int projectId, const QVector<int> &orderedIds);

    // ---- sub tasks ---------------------------------------------------------
    static QVector<SubTask> loadSubTasks(int mainTaskId);
    static int insertSubTask(const SubTask &s);
    static bool updateSubTask(const SubTask &s);
    static bool deleteSubTask(int id);
    static SubTask subTask(int id);

    // main tasks created FROM a given subtask (a subtask may create several)
    static QVector<MainTask> mainTasksFromSubtask(int subTaskId);

    // ---- records -----------------------------------------------------------
    static QVector<Record> loadRecordsForMainTask(int mainTaskId);
    static QVector<Record> loadRecordsForSubTask(int subTaskId);
    static bool addRecord(int mainTaskId, int subTaskId, const QString &content);
    static bool updateRecord(int recordId, const QString &content);
    static bool deleteRecord(int recordId);

    // Apply automatic status transitions (not_started/in_progress/delayed)
    // based on start/end times. Completed/stopped are never touched.
    // Returns number of rows updated.
    static int syncAutoStatuses();

    // Effective status for auto-managed statuses.
    static TaskStatus computeAutoStatus(const QDateTime &start, const QDateTime &end);

private:
};

} // namespace ksat

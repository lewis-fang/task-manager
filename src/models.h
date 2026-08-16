#pragma once

#include <QColor>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace ksat {

// ---- Status ---------------------------------------------------------------
// Statuses: not_started / in_progress / completed / delayed / stopped
//   completed & stopped: set manually, record set time, override everything,
//                        never auto-updated afterwards.
//   not_started / in_progress / delayed: computed automatically from times.
enum class TaskStatus { NotStarted, InProgress, Completed, Delayed, Stopped };

QString statusKey(TaskStatus s);            // "not_started" ...
TaskStatus statusFromKey(const QString &k);

// ---- Entities -------------------------------------------------------------
struct Project {
    int id = -1;
    QString name;
    QColor color = QColor(Qt::white);
};

struct Person {
    int id = -1;
    QString name;
};

struct MainTask {
    int id = -1;
    QString name;
    int projectId = -1;        // -1 = none
    int peopleId = -1;         // -1 = none
    QDateTime startTime;       // invalid = none
    QDateTime endTime;         // invalid = none
    TaskStatus manualStatus = TaskStatus::NotStarted;  // stored status
    QDateTime statusSetTime;   // when completed/stopped was set manually
    int sourceSubtaskId = -1;  // the subtask that created this main task (-1 = none)
    int sortOrder = 0;         // display order within its project group
    QDateTime createdAt;

    // Effective status: completed/stopped are fixed; others auto-derived.
    TaskStatus displayStatus() const;
};

struct SubTask {
    int id = -1;
    int mainTaskId = -1;
    QString name;
    QDateTime startTime;
    QDateTime endTime;
    TaskStatus manualStatus = TaskStatus::NotStarted;
    QDateTime statusSetTime;
    QDateTime createdAt;

    TaskStatus displayStatus() const;
};

struct Record {
    int id = -1;
    int mainTaskId = -1;   // exactly one of mainTaskId / subTaskId is set
    int subTaskId = -1;
    QString content;
    QDateTime createdAt;
};

} // namespace ksat

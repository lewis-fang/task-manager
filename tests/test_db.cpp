// Headless functional test for the KSAT data layer (SQLite).
#include "database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QSet>
#include <QSqlQuery>
#include <QTemporaryDir>

using namespace ksat;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { qCritical() << "FAIL:" << msg; ++failures; } \
    else { qInfo() << "ok:" << msg; } \
} while (0)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // fresh SQLite file in a temp dir
    QTemporaryDir tmp;
    const QString dbFile = tmp.filePath("ksat_test.db");
    const QString err = Database::connect(dbFile);
    CHECK(err.isEmpty(), QStringLiteral("connect: %1").arg(err));
    if (!err.isEmpty()) return 1;

    CHECK(Database::ensureSchema().isEmpty(), "ensureSchema");

    // clean slate for repeatability (FK off so DELETE order does not matter)
    {
        QSqlQuery q(Database::db());
        q.exec("PRAGMA foreign_keys = OFF");
        q.exec("DELETE FROM records");
        q.exec("DELETE FROM sub_tasks");
        q.exec("DELETE FROM main_tasks");
        q.exec("DELETE FROM people");
        q.exec("DELETE FROM projects");
        q.exec("PRAGMA foreign_keys = ON");
    }

    // projects / people
    const int pA = Database::ensureProject("Alpha");
    const int pB = Database::ensureProject("Beta");
    const int pA2 = Database::ensureProject("Alpha");
    CHECK(pA > 0 && pA == pA2, "ensureProject idempotent");
    CHECK(pB > 0 && pB != pA, "distinct projects");
    CHECK(Database::loadProjects().size() == 2, "loadProjects count");

    const int person = Database::ensurePerson("张三");
    CHECK(person > 0 && Database::ensurePerson("张三") == person, "ensurePerson idempotent");

    // main task
    MainTask t;
    t.name = "设计数据库";
    t.projectId = pA;
    t.peopleId = person;
    t.startTime = QDateTime::currentDateTime().addDays(-1);
    t.endTime = QDateTime::currentDateTime().addDays(1);
    const int tId = Database::insertMainTask(t);
    CHECK(tId > 0, "insertMainTask");

    // auto status: in progress (window contains now)
    Database::syncAutoStatuses();
    {
        const MainTask got = Database::mainTask(tId);
        CHECK(got.displayStatus() == TaskStatus::InProgress, "auto status in_progress");
    }

    // future task -> not started
    MainTask t2;
    t2.name = "未来任务";
    t2.projectId = pB;
    t2.startTime = QDateTime::currentDateTime().addDays(5);
    t2.endTime = QDateTime::currentDateTime().addDays(6);
    const int t2Id = Database::insertMainTask(t2);
    Database::syncAutoStatuses();
    CHECK(Database::mainTask(t2Id).displayStatus() == TaskStatus::NotStarted, "auto status not_started");

    // past task -> delayed
    MainTask t3;
    t3.name = "过期任务";
    t3.startTime = QDateTime::currentDateTime().addDays(-5);
    t3.endTime = QDateTime::currentDateTime().addDays(-1);
    const int t3Id = Database::insertMainTask(t3);
    Database::syncAutoStatuses();
    CHECK(Database::mainTask(t3Id).displayStatus() == TaskStatus::Delayed, "auto status delayed");

    // manual completed -> locks, records time, never auto-updates
    MainTask t4;
    t4.name = "已完成";
    t4.startTime = QDateTime::currentDateTime().addDays(-5);
    t4.endTime = QDateTime::currentDateTime().addDays(-1); // would be "delayed"
    t4.manualStatus = TaskStatus::Completed;
    t4.statusSetTime = QDateTime::currentDateTime();
    const int t4Id = Database::insertMainTask(t4);
    Database::syncAutoStatuses();
    {
        const MainTask got = Database::mainTask(t4Id);
        CHECK(got.displayStatus() == TaskStatus::Completed, "manual completed stays completed");
        CHECK(got.statusSetTime.isValid(), "completed has set time");
    }

    // subtasks
    SubTask st;
    st.mainTaskId = tId;
    st.name = "画ER图";
    st.startTime = QDateTime::currentDateTime().addDays(-1);
    st.endTime = QDateTime::currentDateTime().addSecs(2 * 3600);
    const int stId = Database::insertSubTask(st);
    CHECK(stId > 0, "insertSubTask");
    Database::syncAutoStatuses();
    CHECK(Database::subTask(stId).displayStatus() == TaskStatus::InProgress, "subtask auto status");

    // subtask -> create main task (link)
    MainTask fromSub;
    fromSub.name = "画ER图";      // same name as subtask
    fromSub.projectId = pA;       // same project
    fromSub.sourceSubtaskId = stId;
    const int fromSubId = Database::insertMainTask(fromSub);
    CHECK(fromSubId > 0, "insertMainTask from subtask");
    const QVector<MainTask> linked = Database::mainTasksFromSubtask(stId);
    CHECK(linked.size() == 1 && linked.first().id == fromSubId, "subtask link to main task");
    CHECK(Database::mainTask(fromSubId).sourceSubtaskId == stId, "main task source subtask");

    // records
    CHECK(Database::addRecord(tId, -1, "第一条记录"), "add main record");
    CHECK(Database::addRecord(-1, stId, "子任务记录"), "add sub record");
    CHECK(Database::loadRecordsForMainTask(tId).size() == 1, "main records count");
    CHECK(Database::loadRecordsForSubTask(stId).size() == 1, "sub records count");

    // record editing (work order: records 新增编辑功能)
    {
        const QVector<Record> mainRecs = Database::loadRecordsForMainTask(tId);
        const int rid = mainRecs.first().id;
        CHECK(Database::updateRecord(rid, "第一条记录v2"), "updateRecord");
        CHECK(Database::loadRecordsForMainTask(tId).first().content == "第一条记录v2",
              "record content updated");
        const QVector<Record> subRecs = Database::loadRecordsForSubTask(stId);
        CHECK(Database::updateRecord(subRecs.first().id, "子任务记录v2"), "updateRecord (sub)");
        CHECK(Database::loadRecordsForSubTask(stId).first().content == "子任务记录v2",
              "sub record content updated");
    }

    // filters
    CHECK(Database::loadMainTasks({pA}, {}, {}, false).size() >= 2, "filter by project");
    CHECK(Database::loadMainTasks({}, {person}, {}, false).size() >= 1, "filter by people");
    CHECK(Database::loadMainTasks({}, {}, {TaskStatus::Delayed}, false).size() >= 1, "filter by status delayed");

    // completed task hidden unless included
    const QVector<MainTask> active = Database::loadMainTasks({}, {}, {}, false);
    const QVector<MainTask> withDone = Database::loadMainTasks({}, {}, {TaskStatus::Completed}, true);
    bool doneVisible = false;
    for (const MainTask &x : withDone) if (x.id == t4Id) doneVisible = true;
    CHECK(doneVisible, "completed visible when included");
    bool doneHidden = true;
    for (const MainTask &x : active) if (x.id == t4Id) doneHidden = false;
    CHECK(doneHidden, "completed hidden by default");

    // update
    MainTask upd = Database::mainTask(tId);
    upd.name = "设计数据库v2";
    CHECK(Database::updateMainTask(upd), "updateMainTask");
    CHECK(Database::mainTask(tId).name == "设计数据库v2", "name updated");

    // --- sort_order: grouping + reordering (work order: tasks support drag sorting) ---

    // Project A already has: 设计数据库v2 (tId), 画ER图 (fromSubId).
    // Insert more tasks and verify per-group sort_order assignment.
    MainTask a1; a1.name = "A-1"; a1.projectId = pA;
    const int a1Id = Database::insertMainTask(a1);
    MainTask a2; a2.name = "A-2"; a2.projectId = pA;
    const int a2Id = Database::insertMainTask(a2);
    MainTask b1; b1.name = "B-1"; b1.projectId = pB;
    const int b1Id = Database::insertMainTask(b1);
    CHECK(a1Id > 0 && a2Id > 0 && b1Id > 0, "insert extra tasks for sorting");

    // Each project's sort_order must be independent and sequential from 0.
    {
        const QVector<MainTask> all = Database::loadMainTasks({}, {}, {}, true);
        auto sortOf = [&](int id) {
            for (const MainTask &x : all) if (x.id == id) return x.sortOrder;
            return -1;
        };
        QSet<int> aSorts, bSorts;
        for (const MainTask &x : all) {
            if (x.projectId == pA) aSorts.insert(x.sortOrder);
            else if (x.projectId == pB) bSorts.insert(x.sortOrder);
        }
        CHECK(sortOf(a1Id) >= 0 && sortOf(a2Id) >= 0 && sortOf(b1Id) >= 0,
              "sort_order assigned on insert");
        CHECK(aSorts.size() == 4, "project A orders unique (4 tasks)");
        CHECK(bSorts.size() == 2, "project B orders unique (2 tasks)");
    }

    // loadMainTasks must return tasks grouped by project, each group in sort_order.
    {
        const QVector<MainTask> ordered = Database::loadMainTasks({}, {}, {}, true);
        bool grouped = true;
        int seenB = 0;
        for (const MainTask &x : ordered) {
            if (x.projectId == pB) ++seenB;
            else if (seenB > 0) grouped = false; // project A after B already started
        }
        CHECK(grouped, "loadMainTasks groups projects together");

        bool seq = true;
        QMap<int, int> last;
        for (const MainTask &x : ordered) {
            const int key = x.projectId < 0 ? 0 : x.projectId;
            if (last.contains(key) && x.sortOrder < last[key]) seq = false;
            last[key] = x.sortOrder;
        }
        CHECK(seq, "sort_order ascending within each group");
    }

    // reorderMainTasks: swap A-1 and A-2 within project A.
    {
        const QVector<MainTask> aTasks = Database::loadMainTasks({pA}, {}, {}, true);
        QVector<int> ids;
        for (const MainTask &x : aTasks) ids.append(x.id);
        CHECK(ids.contains(a1Id) && ids.contains(a2Id), "reorder: both tasks present");
        std::swap(ids[ids.indexOf(a1Id)], ids[ids.indexOf(a2Id)]);
        CHECK(Database::reorderMainTasks(pA, ids), "reorderMainTasks");
        const QVector<MainTask> after = Database::loadMainTasks({pA}, {}, {}, true);
        int a1Pos = -1, a2Pos = -1;
        for (int i = 0; i < after.size(); ++i) {
            if (after[i].id == a1Id) a1Pos = i;
            if (after[i].id == a2Id) a2Pos = i;
        }
        CHECK(a1Pos > a2Pos, "A-1 now after A-2 (order persisted)");
        // project B untouched
        const QVector<MainTask> bTasks = Database::loadMainTasks({pB}, {}, {}, true);
        CHECK(bTasks.size() == 2, "reorder: project B unaffected");
    }

    // updateMainTask moving a task to another project appends to the new group.
    {
        MainTask mv = Database::mainTask(a1Id);
        mv.projectId = pB;
        CHECK(Database::updateMainTask(mv), "updateMainTask across projects");
        const QVector<MainTask> bAfter = Database::loadMainTasks({pB}, {}, {}, true);
        CHECK(bAfter.last().id == a1Id, "moved task appended at end of new project group");
    }

    // delete
    CHECK(Database::deleteMainTask(fromSubId), "deleteMainTask");
    CHECK(Database::mainTasksFromSubtask(stId).isEmpty(), "link removed after delete");

    qInfo() << (failures == 0 ? "ALL TESTS PASSED" : "FAILURES: " + QString::number(failures));
    return failures == 0 ? 0 : 1;
}

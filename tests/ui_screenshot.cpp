// Headless UI screenshot harness: seeds demo data, shows MainWindow,
// grabs PNGs of the board for visual verification.
#include "database.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>

using namespace ksat;

static void seed()
{
    QSqlQuery q(Database::db());
    q.exec("PRAGMA foreign_keys = OFF");
    q.exec("DELETE FROM records");
    q.exec("DELETE FROM sub_tasks");
    q.exec("DELETE FROM main_tasks");
    q.exec("DELETE FROM people");
    q.exec("DELETE FROM projects");
    q.exec("PRAGMA foreign_keys = ON");

    const int pA = Database::ensureProject("Website Redesign");
    const int pB = Database::ensureProject("Mobile App");
    const int pC = Database::ensureProject("Internal Tools");
    const int p1 = Database::ensurePerson("Zhang San");
    const int p2 = Database::ensurePerson("Li Si");

    auto add = [&](const QString &name, int proj, int people, int startD, int endD,
                   TaskStatus manual, bool withSub) {
        MainTask t;
        t.name = name;
        t.projectId = proj;
        t.peopleId = people;
        if (startD >= 0) t.startTime = QDateTime::currentDateTime().addDays(startD);
        if (endD >= 0) t.endTime = QDateTime::currentDateTime().addDays(endD);
        t.manualStatus = manual;
        if (manual == TaskStatus::Completed || manual == TaskStatus::Stopped)
            t.statusSetTime = QDateTime::currentDateTime().addDays(-2);
        const int id = Database::insertMainTask(t);
        if (withSub) {
            SubTask s;
            s.mainTaskId = id;
            s.name = "Page Structure Mapping";
            s.startTime = QDateTime::currentDateTime().addDays(startD);
            s.endTime = QDateTime::currentDateTime().addDays(endD == 0 ? 1 : endD);
            const int sid = Database::insertSubTask(s);
            // subtask -> main task link demo
            if (name == "Homepage Redesign") {
                MainTask fromSub;
                fromSub.name = "Page Structure Mapping";
                fromSub.projectId = proj;
                fromSub.sourceSubtaskId = sid;
                Database::insertMainTask(fromSub);
            }
            Database::addRecord(-1, sid, "Wireframe draft done");
        }
        Database::addRecord(id, -1, "Visual direction confirmed with designer");
        Database::addRecord(id, -1, "Friday sync");
    };

    // active tasks
    add("Homepage Redesign", pA, p1, -1, 3, TaskStatus::InProgress, true);
    add("Login Flow Rework", pA, p1, 0, 5, TaskStatus::NotStarted, false);
    add("Order Page Adaptation", pB, p2, -3, -1, TaskStatus::Delayed, false);
    add("Push Service", pB, p2, -2, 2, TaskStatus::InProgress, false);
    // finished (hidden on board)
    add("Legacy Data Migration", pC, p1, -10, -8, TaskStatus::Completed, false);
    add("Prototype Review", pA, p2, -5, -4, TaskStatus::Stopped, false);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("KSAT");
    QCoreApplication::setApplicationName("KSAT");

    QTemporaryDir tmp;
    const QString err = Database::connect(tmp.filePath("ksat_ui.db"));
    if (!err.isEmpty()) { qCritical() << "connect failed:" << err; return 1; }
    Database::ensureSchema();
    seed();
    Database::syncAutoStatuses();

    MainWindow w;
    w.resize(1400, 800);
    w.show();
    app.processEvents();

    const QString dir = QStringLiteral("/tmp/opencode/shots");
    QDir().mkpath(dir);

    // light zh
    QFile f1(dir + "/board_light_zh.png");
    if (f1.open(QIODevice::WriteOnly)) {
        w.grab().save(&f1, "PNG");
        f1.close();
        qInfo() << "saved" << f1.fileName();
    }

    app.processEvents();
    return 0;
}

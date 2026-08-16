// End-to-end login gate + DB connect test against the REAL user config.
#include "dialogs.h"
#include "database.h"
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QDebug>
using namespace ksat;
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("KSAT");
    QCoreApplication::setApplicationName("KSAT");
    QSettings s;
    int failures = 0;
    // login gate: the stored hash must match passwordHash("123")
    const QString stored = s.value("auth/passwordHash").toString();
    if (passwordHash("123") != stored) { qCritical() << "FAIL: login 123 rejected"; ++failures; }
    else qInfo() << "ok: login 123 accepted";
    // DB connect with REAL stored config (SQLite file path); default = exe dir
    const QString defaultDir = QCoreApplication::applicationDirPath();
    const QString dbFile = s.value("db/file", QDir(defaultDir).filePath("ksat.db")).toString();
    const QString err = Database::connect(dbFile);
    if (!err.isEmpty()) { qCritical() << "FAIL: DB connect:" << err; ++failures; }
    else qInfo() << "ok: DB connect with stored config";
    qInfo() << (failures == 0 ? "ALL GATE TESTS PASSED" : "GATE TESTS FAILED");
    return failures ? 1 : 0;
}
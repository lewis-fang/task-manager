#include "database.h"
#include "dialogs.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

using namespace ksat;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("KSAT"));
    QCoreApplication::setApplicationName(QStringLiteral("KSAT"));

    QSettings s;

    // --- login gate (first launch initializes the password to "123") ---
    // Runs BEFORE the database connection: the login is the application's
    // own gate. Otherwise, if the DB connection fails, the user never
    // reaches the app login.
    const QString pwdKey = QStringLiteral("auth/passwordHash");
    QString storedHash = s.value(pwdKey).toString();
    if (storedHash.isEmpty()) {
        storedHash = passwordHash(QStringLiteral("123"));
        s.setValue(pwdKey, storedHash);
        QMessageBox::information(nullptr, "KSAT",
            QStringLiteral("First launch: the initial password is set to 123. Please change it after logging in."));
    }
    for (;;) {
        LoginDialog dlg;
        if (dlg.exec() != QDialog::Accepted) {
            QMessageBox::information(nullptr, "KSAT",
                QStringLiteral("Login cancelled; exiting."));
            return 0;
        }
        if (passwordHash(dlg.password()) == storedHash) break;
        QMessageBox::warning(nullptr, "KSAT",
            QStringLiteral("Wrong password; please try again."));
    }

    // --- SQLite database (self-contained single file) ---
    // Default location: the directory of the KSAT executable (portable).
    //   Linux/Windows: <exe-dir>/ksat.db
    const QString defaultDir = QCoreApplication::applicationDirPath();
    const QString dbPath =
        s.value(QStringLiteral("db/file"), QDir(defaultDir).filePath(QStringLiteral("ksat.db")))
            .toString();

    const QString connError = Database::connect(dbPath);
    if (!connError.isEmpty()) {
        QMessageBox::critical(nullptr, "KSAT",
            QStringLiteral("Cannot open the database file.\n\n%1\n\n%2")
                .arg(dbPath, connError));
        return 1;
    }
    const QString schemaErr = Database::ensureSchema();
    if (!schemaErr.isEmpty()) {
        QMessageBox::critical(nullptr, "KSAT",
            QStringLiteral("Database setup failed:\n\n%1").arg(schemaErr));
        return 1;
    }

    MainWindow w;
    w.show();
    return app.exec();
}
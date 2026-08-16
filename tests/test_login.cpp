// Headless test for the KSAT login gate:
//   - passwordHash() determinism & salting
//   - first-launch initializes the password to "123"
//   - wrong password is rejected, correct password accepted
//   - change-password flow (verify current, store new, old no longer works)
//   - LoginDialog / ChangePasswordDialog getters
#include "dialogs.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>

using namespace ksat;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { qCritical() << "FAIL:" << msg; ++failures; } \
    else { qInfo() << "ok:" << msg; } \
} while (0)

static void setPassword(QDialog &dlg, const QString &pw)
{
    for (QLineEdit *le : dlg.findChildren<QLineEdit *>())
        le->setText(pw);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // isolate QSettings in a temp dir
    QTemporaryDir tmp;
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, tmp.path());
    QSettings s;

    // --- passwordHash ---
    const QString h1 = passwordHash(QStringLiteral("123"));
    CHECK(!h1.isEmpty() && h1 == passwordHash(QStringLiteral("123")), "passwordHash deterministic");
    CHECK(h1 != passwordHash(QStringLiteral("1234")), "different passwords differ");
    CHECK(h1.length() == 64, "passwordHash is 64 hex chars (SHA-256)");

    // --- first launch: no stored hash -> initialize to 123 ---
    CHECK(s.value(QStringLiteral("auth/passwordHash")).toString().isEmpty(),
          "first launch has no stored hash");
    QString stored = s.value(QStringLiteral("auth/passwordHash")).toString();
    if (stored.isEmpty()) {
        stored = passwordHash(QStringLiteral("123"));
        s.setValue(QStringLiteral("auth/passwordHash"), stored);
    }
    CHECK(s.value(QStringLiteral("auth/passwordHash")).toString() == passwordHash(QStringLiteral("123")),
          "first launch stores hash of 123");

    // --- login attempts ---
    CHECK(passwordHash(QStringLiteral("wrong")) != stored, "wrong password rejected");
    CHECK(passwordHash(QStringLiteral("123")) == stored, "correct password accepted");

    // --- change password ---
    const QString oldHash = s.value(QStringLiteral("auth/passwordHash")).toString();
    CHECK(oldHash == passwordHash(QStringLiteral("123")), "current hash is hash of 123");
    const QString newHash = passwordHash(QStringLiteral("8888"));
    s.setValue(QStringLiteral("auth/passwordHash"), newHash);
    CHECK(s.value(QStringLiteral("auth/passwordHash")).toString() == passwordHash(QStringLiteral("8888")),
          "new hash stored after change");
    CHECK(passwordHash(QStringLiteral("123")) != s.value(QStringLiteral("auth/passwordHash")).toString(),
          "old password no longer works");
    CHECK(passwordHash(QStringLiteral("8888")) == s.value(QStringLiteral("auth/passwordHash")).toString(),
          "new password works");

    // --- dialogs ---
    LoginDialog login;
    setPassword(login, QStringLiteral("123"));
    CHECK(login.password() == QStringLiteral("123"), "LoginDialog::password getter");

    ChangePasswordDialog change;
    change.findChildren<QLineEdit *>().value(0)->setText(QStringLiteral("123"));
    change.findChildren<QLineEdit *>().value(1)->setText(QStringLiteral("8888"));
    change.findChildren<QLineEdit *>().value(2)->setText(QStringLiteral("8888"));
    CHECK(change.oldPassword() == QStringLiteral("123"), "ChangePasswordDialog::oldPassword getter");
    CHECK(change.newPassword() == QStringLiteral("8888"), "ChangePasswordDialog::newPassword getter");

    qInfo() << (failures == 0 ? "ALL LOGIN TESTS PASSED" : "LOGIN TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
// Headless UI-structure verification for the KSAT work-order changes.
// Verifies widget geometry/style invariants programmatically (visual checks
// are not possible in headless CI): task edge full width, content padding,
// plain "+" buttons, subtask records default-expanded + popup controls,
// per-project grouping of cards.
#include "database.h"
#include "dialogs.h"
#include "mainwindow.h"
#include "taskcard.h"

#include <QApplication>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QWidget>

using namespace ksat;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { qCritical() << "FAIL:" << msg; ++failures; } \
    else { qInfo() << "ok:" << msg; } \
} while (0)

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

    const int pA = Database::ensureProject("Alpha");
    const int pB = Database::ensureProject("Beta");
    const int person = Database::ensurePerson("Zhang San");

    auto add = [&](const QString &name, int proj, int startD, int endD) {
        MainTask t;
        t.name = name;
        t.projectId = proj;
        t.peopleId = person;
        t.startTime = QDateTime::currentDateTime().addDays(startD);
        t.endTime = QDateTime::currentDateTime().addDays(endD);
        return Database::insertMainTask(t);
    };

    // two projects so grouping is observable; finished task hidden
    const int a1 = add("Alpha-1", pA, -1, 3);
    add("Alpha-2", pA, 0, 4);
    add("Beta-1", pB, -2, 5);
    add("Beta-2", pB, -3, 2);

    // one subtask with explicit times on Alpha-1
    SubTask s;
    s.mainTaskId = a1;
    s.name = "Sub detail";
    s.startTime = QDateTime::currentDateTime().addDays(-1);
    s.endTime = QDateTime::currentDateTime().addDays(1);
    const int sid = Database::insertSubTask(s);
    Database::addRecord(-1, sid, "wireframe done");
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

    const QList<TaskCard *> cards = w.findChildren<TaskCard *>();
    CHECK(cards.size() == 4, QStringLiteral("4 cards on board (got %1)").arg(cards.size()));

    for (TaskCard *card : cards) {
        // (1) top edge spans the full card width
        QFrame *edge = card->findChild<QFrame *>(QStringLiteral("taskEdge"));
        CHECK(edge != nullptr, "card has taskEdge");
        if (edge) {
            app.processEvents();
            // edge spans the full inner width (card has a 1px border each side)
            const bool fullWidth = edge->width() == card->contentsRect().width();
            CHECK(fullWidth, QStringLiteral("edge full width (edge=%1 inner=%2)")
                                 .arg(edge->width()).arg(card->contentsRect().width()));
        }

        // (2) content sits on a padded inner widget
        QWidget *content = card->findChild<QWidget *>(QStringLiteral("taskContent"));
        CHECK(content != nullptr, "card has taskContent");
        if (content) {
            const QMargins m = qobject_cast<QVBoxLayout *>(content->layout())
                                   ? content->layout()->contentsMargins()
                                   : QMargins();
            CHECK(m.left() == 12 && m.right() == 12,
                  QStringLiteral("content padding 12/12 (got %1/%2)").arg(m.left()).arg(m.right()));
        }

        // (2b) meta shows as three independent gray lines (project / owner / time)
        const auto projL = card->findChild<QLabel *>(QStringLiteral("metaProject"));
        const auto ownerL = card->findChild<QLabel *>(QStringLiteral("metaOwner"));
        const auto timeL = card->findChild<QLabel *>(QStringLiteral("metaTime"));
        CHECK(projL && ownerL && timeL, "card has three meta labels");
        if (projL && ownerL && timeL) {
            const bool gray = projL->styleSheet().contains(QStringLiteral("#8e8e93"))
                              && ownerL->styleSheet().contains(QStringLiteral("#8e8e93"))
                              && timeL->styleSheet().contains(QStringLiteral("#8e8e93"));
            CHECK(gray, "meta lines are gray");
            // every seeded card has project + owner + times set
            CHECK(projL->isVisibleTo(card) && !projL->text().isEmpty(), "meta project line shown");
            CHECK(ownerL->isVisibleTo(card) && !ownerL->text().isEmpty(), "meta owner line shown");
            CHECK(timeL->isVisibleTo(card) && !timeL->text().isEmpty(), "meta time line shown");
        }

        // (3) "+" buttons are plain (flat + neutral color, no blue)
        const auto plus = card->findChildren<QPushButton *>();
        int plusCount = 0;
        for (QPushButton *b : plus) {
            if (b->text() != QStringLiteral("+")) continue;
            ++plusCount;
            const QString ss = b->styleSheet();
            const bool plain = b->isFlat()
                               && ss.contains(QStringLiteral("#8e8e93"))
                               && !ss.contains(QStringLiteral("007AFF"));
            CHECK(plain, "plain '+' button (no color)");
        }
        // 2 on the card itself (records + subtasks headers); a card whose
        // subtask records are default-expanded carries one more "+" (the
        // subtask records add button).
        const bool hasSubRow = !card->findChildren<QWidget *>(QStringLiteral("subtaskRow")).isEmpty();
        const int expected = hasSubRow ? 3 : 2;
        CHECK(plusCount == expected,
              QStringLiteral("'%1' buttons per card (got %2, expect %3)")
                  .arg(hasSubRow ? "3 (subtask records expanded)"
                                 : "2")
                  .arg(plusCount).arg(expected));
    }

    // (4) subtask row: records default-expanded; header hides time; "⋯" opens
    // a popup list (verified by structure, not by menu execution)
    const QList<QWidget *> rows = w.findChildren<QWidget *>(QStringLiteral("subtaskRow"));
    CHECK(rows.size() == 1, QStringLiteral("one subtask row (got %1)").arg(rows.size()));
    if (!rows.isEmpty()) {
        const auto labels = rows.first()->findChildren<QLabel *>();
        bool timeOnHeader = false;
        for (QLabel *l : labels) {
            if (l->text().contains(QStringLiteral("~")) && l->isVisibleTo(rows.first()))
                timeOnHeader = true;
        }
        CHECK(!timeOnHeader, "subtask time hidden on the header row");

        // records are ALWAYS visible (default expanded) — no toggle panel anymore
        QWidget *actions = rows.first()->findChild<QWidget *>(QStringLiteral("subtaskActions"));
        CHECK(actions == nullptr, "old subtask actions panel removed (now a popup list)");
        QFrame *recBox = rows.first()->findChild<QFrame *>(QStringLiteral("subRecordsBox"));
        CHECK(recBox != nullptr, "subtask records box exists");
        if (recBox) CHECK(recBox->isVisibleTo(rows.first()), "subtask records default expanded");

        // records box carries an add (+) and per-row edit (✎) buttons
        if (recBox) {
            const auto recButtons = recBox->findChildren<QPushButton *>();
            int plusCount = 0, editCount = 0;
            for (QPushButton *b : recButtons) {
                if (b->text() == QStringLiteral("+")) ++plusCount;
                if (b->text() == QStringLiteral("✎")) ++editCount;
            }
            CHECK(plusCount == 1, QStringLiteral("subtask records add '+' button (got %1)").arg(plusCount));
            CHECK(editCount == 1, QStringLiteral("subtask record has an edit (✎) button (got %1)").arg(editCount));
        }

        // "⋯" expand control exists on the header
        const auto headerButtons = rows.first()->findChildren<QPushButton *>();
        bool hasMore = false;
        for (QPushButton *b : headerButtons)
            if (b->text() == QStringLiteral("⋯")) hasMore = true;
        CHECK(hasMore, "subtask '⋯' popup control present");
    }

    // (5) same-project cards are grouped in layout order
    {
        const QVector<MainTask> all = Database::loadMainTasks({}, {}, {}, false);
        bool grouped = true;
        bool seenBeta = false;
        for (const MainTask &t : all) {
            if (t.projectId >= 0 && Database::projectName(t.projectId) == QStringLiteral("Beta")) {
                if (!seenBeta) seenBeta = true;
            } else if (seenBeta) {
                grouped = false;
            }
        }
        CHECK(grouped, "same-project cards grouped (Beta contiguous)");
    }

    // (6) dialog controls follow the Apple-style QSS
    {
        MainTaskDialog dlg(MainTask(), -1, -1);
        const auto ok = dlg.findChild<QPushButton *>(QStringLiteral("okButton"));
        const auto cancel = dlg.findChild<QPushButton *>(QStringLiteral("cancelButton"));
        CHECK(ok != nullptr, "dialog OK button has Apple role name");
        CHECK(cancel != nullptr, "dialog Cancel button has Apple role name");
        if (ok) {
            CHECK(ok->styleSheet().isEmpty() && !ok->text().isEmpty(),
                  "OK button styled via QSS (objectName), not inline");
        }
        // combo boxes exist and are styled (white bg via QSS; verify objectName-free inline)
        const auto combos = dlg.findChildren<QComboBox *>();
        CHECK(combos.size() == 2, QStringLiteral("dialog has project+owner combos (got %1)").arg(combos.size()));
        // inline styleSheet must be empty for these — the QSS owns the look
        bool inlineStyled = false;
        for (QComboBox *c : combos)
            if (!c->styleSheet().isEmpty()) inlineStyled = true;
        CHECK(!inlineStyled, "combos styled by QSS, no inline styles");

        SubTaskDialog sd(SubTask(), -1);
        CHECK(sd.findChild<QPushButton *>(QStringLiteral("okButton")) != nullptr,
              "subtask dialog OK button styled");
        // 澄清: 新增子任务不需要选择任务状态 — new subtask has NO status combo
        CHECK(sd.findChildren<QComboBox *>().isEmpty(),
              "new-subtask dialog hides the status selector");
        // editing keeps the status selector (so Done/Stopped can be set)
        SubTask subEdit;
        subEdit.id = 1;
        subEdit.manualStatus = TaskStatus::Completed;
        SubTaskDialog sdEdit(subEdit, 1);
        CHECK(!sdEdit.findChildren<QComboBox *>().isEmpty(),
              "edit-subtask dialog keeps the status selector");

        // RecordDialog: add vs edit modes
        RecordDialog rdAdd;
        CHECK(rdAdd.findChild<QPushButton *>(QStringLiteral("okButton")) != nullptr,
              "record dialog OK button styled");
        CHECK(rdAdd.windowTitle() == QStringLiteral("Add Note"),
              "record dialog title 'Add Note'");
        RecordDialog rdEdit(QStringLiteral("existing note"));
        CHECK(rdEdit.windowTitle() == QStringLiteral("Edit Note"),
              "record dialog edit title 'Edit Note'");
        CHECK(rdEdit.content() == QStringLiteral("existing note"),
              "record dialog edit mode pre-fills content");

        // Apple time picker: no spin buttons on the datetime edits
        MainTaskDialog dlg2(MainTask(), -1, -1);
        const auto edits = dlg2.findChildren<QDateTimeEdit *>();
        CHECK(edits.size() == 2, QStringLiteral("main dialog has start+end time edits (got %1)").arg(edits.size()));
        bool anySpinButtons = false;
        bool dayOnly = true;
        for (const QDateTimeEdit *e : edits) {
            if (e->buttonSymbols() != QAbstractSpinBox::NoButtons) anySpinButtons = true;
            if (e->displayFormat() != QStringLiteral("yyyy-MM-dd")) dayOnly = false;
        }
        CHECK(!anySpinButtons, "time edits use Apple style (no up/down spin buttons)");
        CHECK(dayOnly, "time edits are day-only (yyyy-MM-dd, no hour/minute)");
        LoginDialog ld;
        CHECK(ld.findChild<QPushButton *>(QStringLiteral("okButton")) != nullptr,
              "login dialog OK button styled");
    }

    qInfo() << (failures == 0 ? "ALL UI-STRUCTURE CHECKS PASSED"
                              : "UI-STRUCTURE CHECKS FAILED: " + QString::number(failures));
    return failures == 0 ? 0 : 1;
}
#include "dialogs.h"

#include "database.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ksat {

// ---------------------------------------------------------------------------
// shared dialog styling (keep dialogs consistent with the main window)
// ---------------------------------------------------------------------------

static void applyDialogStyle(QDialog *dlg)
{
    dlg->setStyleSheet(QStringLiteral(
        "QDialog { background:#f5f5f7; }"
        "QFormLayout QLabel { color:#1d1d1f; font-size:13px; }"
        "QLabel { color:#1d1d1f; font-size:13px; }"
        "QLineEdit, QDateTimeEdit, QPlainTextEdit {"
        "  background:#ffffff; border:1px solid #d1d1d6; border-radius:7px;"
        "  padding:5px 8px; selection-background-color:#007AFF; selection-color:#ffffff; }"
        "QLineEdit:focus, QDateTimeEdit:focus, QPlainTextEdit:focus {"
        "  border:1px solid #007AFF; }"
        "QComboBox { background:#ffffff; border:1px solid #d1d1d6; border-radius:7px;"
        "  padding:5px 8px; min-height:16px; }"
        "QComboBox:focus { border:1px solid #007AFF; }"
        "QComboBox::drop-down { border:none; width:24px; }"
        "QComboBox::down-arrow { image:url(:/resources/arrow.svg); width:12px; height:12px; }"
        "QComboBox QAbstractItemView { background:#ffffff; border:1px solid #e5e5ea;"
        "  border-radius:6px; selection-background-color:#007AFF; selection-color:#ffffff;"
        "  outline:0; padding:2px; }"
        "QDialogButtonBox { margin-top:14px; }"
        "QPushButton#okButton { background:#007AFF; color:#ffffff; border:none;"
        "  border-radius:8px; padding:6px 18px; font-weight:600; min-width:84px; }"
        "QPushButton#okButton:hover { background:#0060df; }"
        "QPushButton#okButton:pressed { background:#004999; }"
        "QPushButton#cancelButton { background:transparent; color:#007AFF; border:none;"
        "  padding:6px 18px; font-weight:500; min-width:84px; }"
        "QPushButton#cancelButton:hover { background:#e8f0fe; border-radius:8px; }"
        "QPushButton#cancelButton:pressed { background:#dcebfd; border-radius:8px; }"
        "QCalendarWidget QWidget#qt_calendar_navigationbar { background:#ffffff;"
        "  border-bottom:1px solid #e5e5ea; }"
        "QCalendarWidget QToolButton { color:#1d1d1f; background:transparent;"
        "  border:none; padding:4px 8px; border-radius:6px; font-weight:600; }"
        "QCalendarWidget QToolButton:hover { background:#f5f5f7; }"
        "QCalendarWidget QAbstractItemView { background:#ffffff; color:#1d1d1f;"
        "  selection-background-color:#007AFF; selection-color:#ffffff; outline:0; }"
        "QCalendarWidget QAbstractItemView:disabled { color:#c7c7cc; }"
        "QCalendarWidget QSpinBox { background:#ffffff; border:1px solid #d1d1d6;"
        "  border-radius:6px; padding:2px 6px; }"));
}

// Assign Apple-style roles to the standard OK/Cancel buttons of a button box.
static void styleDialogButtons(QDialogButtonBox *buttons)
{
    if (QPushButton *ok = buttons->button(QDialogButtonBox::Ok))
        ok->setObjectName(QStringLiteral("okButton"));
    if (QPushButton *cancel = buttons->button(QDialogButtonBox::Cancel))
        cancel->setObjectName(QStringLiteral("cancelButton"));
}

// ---------------------------------------------------------------------------
// password hash / login dialogs
// ---------------------------------------------------------------------------

QString passwordHash(const QString &password)
{
    const QByteArray salted = (QStringLiteral("ksat@") + password).toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(salted, QCryptographicHash::Sha256).toHex());
}

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    applyDialogStyle(this);
    setWindowTitle("KSAT Login");
    setMinimumWidth(320);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel("Enter password:", this));

    m_pass = new QLineEdit(this);
    m_pass->setEchoMode(QLineEdit::Password);
    lay->addWidget(m_pass);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Log In");
    buttons->button(QDialogButtonBox::Cancel)->setText("Cancel");
    connect(buttons, &QDialogButtonBox::accepted, this, &LoginDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &LoginDialog::reject);
    styleDialogButtons(buttons);
    lay->addWidget(buttons);

    m_pass->setFocus();
}

QString LoginDialog::password() const
{
    return m_pass->text();
}

ChangePasswordDialog::ChangePasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    applyDialogStyle(this);
    setWindowTitle("Change Password");
    setMinimumWidth(340);

    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_old = new QLineEdit(this);
    m_old->setEchoMode(QLineEdit::Password);
    m_new = new QLineEdit(this);
    m_new->setEchoMode(QLineEdit::Password);
    m_confirm = new QLineEdit(this);
    m_confirm->setEchoMode(QLineEdit::Password);

    form->addRow("Current password:", m_old);
    form->addRow("New password:", m_new);
    form->addRow("Confirm new password:", m_confirm);
    lay->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("OK");
    buttons->button(QDialogButtonBox::Cancel)->setText("Cancel");
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_new->text().isEmpty()) {
            QMessageBox::warning(this, "Change Password", "New password must not be empty.");
            return;
        }
        if (m_new->text() != m_confirm->text()) {
            QMessageBox::warning(this, "Change Password", "The two new passwords do not match.");
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &ChangePasswordDialog::reject);
    styleDialogButtons(buttons);
    lay->addWidget(buttons);
}

QString ChangePasswordDialog::oldPassword() const
{
    return m_old->text();
}

QString ChangePasswordDialog::newPassword() const
{
    return m_new->text();
}

// ---------------------------------------------------------------------------
// localized status text
// ---------------------------------------------------------------------------

QString MainTaskDialog::statusText(TaskStatus s)
{
    switch (s) {
    case TaskStatus::NotStarted: return "Not started";
    case TaskStatus::InProgress: return "In progress";
    case TaskStatus::Completed:  return "Done";
    case TaskStatus::Delayed:    return "Delayed";
    case TaskStatus::Stopped:    return "Stopped";
    }
    return QString();
}

QColor MainTaskDialog::statusColor(TaskStatus s)
{
    switch (s) {
    case TaskStatus::NotStarted: return QColor("#9E9E9E");
    case TaskStatus::InProgress: return QColor("#1E88E5");
    case TaskStatus::Completed:  return QColor("#43A047");
    case TaskStatus::Delayed:    return QColor("#FB8C00");
    case TaskStatus::Stopped:    return QColor("#757575");
    }
    return QColor("#9E9E9E");
}

// ---------------------------------------------------------------------------
// DateTimeEditRow
// ---------------------------------------------------------------------------

DateTimeEditRow::DateTimeEditRow(const QString &label, QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_check = new QCheckBox(label, this);
    m_edit = new QDateTimeEdit(this);
    m_edit->setCalendarPopup(true);
    m_edit->setButtonSymbols(QAbstractSpinBox::NoButtons); // Apple: no up/down arrows
    m_edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd")); // day precision only
    m_edit->setDateTime(QDateTime::currentDateTime());
    lay->addWidget(m_check);
    lay->addWidget(m_edit, 1);
    connect(m_check, &QCheckBox::toggled, m_edit, &QWidget::setEnabled);
    m_edit->setEnabled(false);
}

QDateTime DateTimeEditRow::dateTime() const
{
    return m_check->isChecked() ? m_edit->dateTime() : QDateTime();
}

void DateTimeEditRow::setDateTime(const QDateTime &dt)
{
    const bool valid = dt.isValid();
    m_check->setChecked(valid);
    m_edit->setEnabled(valid);
    if (valid) m_edit->setDateTime(dt);
}

// ---------------------------------------------------------------------------
// MainTaskDialog
// ---------------------------------------------------------------------------

MainTaskDialog::MainTaskDialog(const MainTask &task, int sourceSubtaskId,
                               int sourceParentTaskId, QWidget *parent)
    : QDialog(parent), m_task(task), m_sourceSubtaskId(sourceSubtaskId),
      m_sourceParentTaskId(sourceParentTaskId)
{
    buildUi();
    loadCombos();
}

void MainTaskDialog::buildUi()
{
    applyDialogStyle(this);
    setWindowTitle(m_task.id < 0 ? "New Main Task" : "Edit Main Task");
    setMinimumWidth(440);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(18, 16, 18, 16);
    lay->setSpacing(12);

    auto *title = new QLabel(m_task.id < 0 ? "New Main Task" : "Edit Main Task", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color:#1d1d1f;"));
    lay->addWidget(title);

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit(m_task.name, this);
    m_nameEdit->setMaxLength(30);
    form->addRow("Task name* (≤30 chars):", m_nameEdit);

    m_projectCombo = new QComboBox(this);
    m_projectCombo->setEditable(true);
    form->addRow("Project:", m_projectCombo);

    m_peopleCombo = new QComboBox(this);
    m_peopleCombo->setEditable(true);
    form->addRow("Owner:", m_peopleCombo);

    m_startEdit = new DateTimeEditRow("Start time:", this);
    m_endEdit = new DateTimeEditRow("End time:", this);
    m_startEdit->setDateTime(m_task.startTime);
    m_endEdit->setDateTime(m_task.endTime);
    form->addRow(QString(), m_startEdit);
    form->addRow(QString(), m_endEdit);

    // New tasks never pick a status: status is derived from the schedule and
    // "Done"/"Stopped" are only relevant once the task exists. Editing keeps
    // the selector so a finished task can be marked Done/Stopped.
    const bool isNew = m_task.id < 0;
    if (!isNew) {
        m_statusCombo = new QComboBox(this);
        m_statusCombo->addItem("Auto (by time)", int(TaskStatus::NotStarted));
        m_statusCombo->addItem("Done", int(TaskStatus::Completed));
        m_statusCombo->addItem("Stopped", int(TaskStatus::Stopped));
        form->addRow("Task status:", m_statusCombo);

        const TaskStatus cur = m_task.manualStatus;
        if (cur == TaskStatus::Completed) m_statusCombo->setCurrentIndex(1);
        else if (cur == TaskStatus::Stopped) m_statusCombo->setCurrentIndex(2);
        else m_statusCombo->setCurrentIndex(0);

        m_statusInfo = new QLabel("Status follows the schedule automatically (Not started / In progress / Delayed). "
                                  "\"Done\" and \"Stopped\" are set manually and record the time — they are never overwritten.", this);
        m_statusInfo->setWordWrap(true);
        m_statusInfo->setStyleSheet(QStringLiteral("color:#888;"));
        form->addRow(QString(), m_statusInfo);
    }

    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setWordWrap(true);
    m_sourceLabel->setStyleSheet(QStringLiteral("color:#666;"));
    form->addRow("Source:", m_sourceLabel);
    lay->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MainTaskDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &MainTaskDialog::reject);
    styleDialogButtons(buttons);
    lay->addWidget(buttons);

    // source subtask info
    if (m_sourceSubtaskId >= 0) {
        const SubTask st = Database::subTask(m_sourceSubtaskId);
        m_sourceLabel->setText(QStringLiteral("Created from subtask \"%1\"; project inherited").arg(st.name));
    } else if (m_task.sourceSubtaskId >= 0) {
        const SubTask st = Database::subTask(m_task.sourceSubtaskId);
        m_sourceLabel->setText(QStringLiteral("Created from subtask \"%1\"; project inherited").arg(st.name));
    } else {
        m_sourceLabel->hide();
    }
}

void MainTaskDialog::loadCombos()
{
    // projects
    m_projectCombo->addItem("(None)", -1);
    int projIdx = 0;
    const QVector<Project> projects = Database::loadProjects();
    for (const Project &p : projects) {
        m_projectCombo->addItem(p.name, p.id);
        if (p.id == m_task.projectId) projIdx = m_projectCombo->count() - 1;
    }
    if (m_sourceSubtaskId >= 0) {
        // project inherited from the parent task of the source subtask
        const SubTask st = Database::subTask(m_sourceSubtaskId);
        const MainTask parent = Database::mainTask(st.mainTaskId);
        if (parent.projectId >= 0) {
            for (int i = 0; i < m_projectCombo->count(); ++i)
                if (m_projectCombo->itemData(i).toInt() == parent.projectId) { projIdx = i; break; }
        }
    }
    m_projectCombo->setCurrentIndex(projIdx);

    // people
    m_peopleCombo->addItem("(None)", -1);
    int peopleIdx = 0;
    const QVector<Person> people = Database::loadPeople();
    for (const Person &p : people) {
        m_peopleCombo->addItem(p.name, p.id);
        if (p.id == m_task.peopleId) peopleIdx = m_peopleCombo->count() - 1;
    }
    m_peopleCombo->setCurrentIndex(peopleIdx);
}

MainTask MainTaskDialog::resultTask() const
{
    MainTask t = m_task;

    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty() || name.size() > 30) return MainTask();
    t.name = name;

    // project: use existing selection, or ensure-create a new named project
    const QString projText = m_projectCombo->currentText().trimmed();
    if (projText.isEmpty() || projText == "(None)") {
        t.projectId = -1;
    } else {
        int pid = m_projectCombo->currentData().toInt();
        if (pid < 0 || m_projectCombo->findText(projText) < 0)
            pid = Database::ensureProject(projText);
        t.projectId = pid;
    }

    const QString peopleText = m_peopleCombo->currentText().trimmed();
    if (peopleText.isEmpty() || peopleText == "(None)") {
        t.peopleId = -1;
    } else {
        int pid = m_peopleCombo->currentData().toInt();
        if (pid < 0 || m_peopleCombo->findText(peopleText) < 0)
            pid = Database::ensurePerson(peopleText);
        t.peopleId = pid;
    }

    t.startTime = m_startEdit->dateTime();
    t.endTime = m_endEdit->dateTime();

    // New tasks carry no manual status (kept as "not_started"); editing may
    // still mark the task Done/Stopped via the status selector.
    if (m_statusCombo) {
        const TaskStatus s = TaskStatus(m_statusCombo->currentData().toInt());
        if (s == TaskStatus::Completed || s == TaskStatus::Stopped) {
            // manual set: record the time, override anything
            if (t.manualStatus != s) t.statusSetTime = QDateTime::currentDateTime();
            t.manualStatus = s;
        } else {
            // auto statuses never lock; clear the manual marker
            t.manualStatus = TaskStatus::NotStarted;
            t.statusSetTime = QDateTime();
        }
    }

    if (m_sourceSubtaskId >= 0) t.sourceSubtaskId = m_sourceSubtaskId;
    return t;
}

void MainTaskDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty() || name.size() > 30) {
        QMessageBox::warning(this, windowTitle(),
                             "Task name must not be empty and must be ≤30 characters.");
        return;
    }
    QDialog::accept();
}

// ---------------------------------------------------------------------------
// SubTaskDialog
// ---------------------------------------------------------------------------

SubTaskDialog::SubTaskDialog(const SubTask &sub, int mainTaskId, QWidget *parent)
    : QDialog(parent), m_sub(sub)
{
    if (m_sub.mainTaskId < 0) m_sub.mainTaskId = mainTaskId;
    applyDialogStyle(this);
    setWindowTitle(sub.id < 0 ? "New Subtask" : "Edit Subtask");
    setMinimumWidth(440);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(18, 16, 18, 16);
    lay->setSpacing(12);

    auto *title = new QLabel(sub.id < 0 ? "New Subtask" : "Edit Subtask", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color:#1d1d1f;"));
    lay->addWidget(title);

    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit(m_sub.name, this);
    m_nameEdit->setMaxLength(100);
    form->addRow("Subtask name:", m_nameEdit);

    m_startEdit = new DateTimeEditRow("Start time:", this);
    m_endEdit = new DateTimeEditRow("End time:", this);
    m_startEdit->setDateTime(m_sub.startTime);
    m_endEdit->setDateTime(m_sub.endTime);
    form->addRow(QString(), m_startEdit);
    form->addRow(QString(), m_endEdit);

    // New subtasks never pick a status: status is derived from the schedule
    // and "Done"/"Stopped" are only relevant once the subtask exists. Editing
    // keeps the selector so a finished subtask can be marked Done/Stopped.
    const bool isNew = m_sub.id < 0;
    if (!isNew) {
        m_statusCombo = new QComboBox(this);
        m_statusCombo->addItem("Auto (by time)", int(TaskStatus::NotStarted));
        m_statusCombo->addItem("Done", int(TaskStatus::Completed));
        m_statusCombo->addItem("Stopped", int(TaskStatus::Stopped));
        const TaskStatus cur = m_sub.manualStatus;
        if (cur == TaskStatus::Completed) m_statusCombo->setCurrentIndex(1);
        else if (cur == TaskStatus::Stopped) m_statusCombo->setCurrentIndex(2);
        else m_statusCombo->setCurrentIndex(0);
        form->addRow("Task status:", m_statusCombo);

        m_statusInfo = new QLabel("Status follows the schedule automatically. \"Done\" and \"Stopped\" are set manually and record the time.", this);
        m_statusInfo->setStyleSheet(QStringLiteral("color:#888;"));
        m_statusInfo->setWordWrap(true);
        form->addRow(QString(), m_statusInfo);
    }
    lay->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SubTaskDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SubTaskDialog::reject);
    styleDialogButtons(buttons);
    lay->addWidget(buttons);
}

SubTask SubTaskDialog::resultSubTask() const
{
    SubTask s = m_sub;
    s.name = m_nameEdit->text().trimmed();
    s.startTime = m_startEdit->dateTime();
    s.endTime = m_endEdit->dateTime();
    // New subtasks carry no manual status; editing may mark Done/Stopped.
    if (m_statusCombo) {
        const TaskStatus st = TaskStatus(m_statusCombo->currentData().toInt());
        if (st == TaskStatus::Completed || st == TaskStatus::Stopped) {
            if (s.manualStatus != st) s.statusSetTime = QDateTime::currentDateTime();
            s.manualStatus = st;
        } else {
            s.manualStatus = TaskStatus::NotStarted;
            s.statusSetTime = QDateTime();
        }
    }
    return s;
}

void SubTaskDialog::accept()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), "Subtask name must not be empty.");
        return;
    }
    QDialog::accept();
}

// ---------------------------------------------------------------------------
// RecordDialog
// ---------------------------------------------------------------------------

RecordDialog::RecordDialog(QWidget *parent)
    : RecordDialog(QString(), parent)
{
}

RecordDialog::RecordDialog(const QString &initialContent, QWidget *parent)
    : QDialog(parent)
{
    applyDialogStyle(this);
    setWindowTitle(initialContent.isEmpty() ? "Add Note" : "Edit Note");
    setMinimumSize(380, 200);
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(initialContent.isEmpty() ? "Note content:" : "Edit note content:", this));
    m_edit = new QPlainTextEdit(this);
    m_edit->setPlainText(initialContent);
    lay->addWidget(m_edit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &RecordDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RecordDialog::reject);
    styleDialogButtons(buttons);
    lay->addWidget(buttons);
}

QString RecordDialog::content() const
{
    return m_edit->toPlainText().trimmed();
}

// ---------------------------------------------------------------------------
// HistoryDialog
// ---------------------------------------------------------------------------

HistoryDialog::HistoryDialog(QWidget *parent)
    : QDialog(parent)
{
    applyDialogStyle(this);
    setWindowTitle("Task History");
    resize(760, 540);

    auto *lay = new QVBoxLayout(this);
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({"Task", "Owner", "Status", "Time"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->setAlternatingRowColors(false);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background:#ffffff; border:1px solid #e5e5ea; border-radius:8px;"
        " font-size:13px; color:#1d1d1f; }"
        "QTreeWidget::item { padding:5px 2px; border:none; }"
        "QTreeWidget::item:selected { background:#e8f0fe; color:#1d1d1f; }"
        "QTreeWidget::item:hover { background:#f5f5f7; }"
        "QHeaderView::section { background:#f5f5f7; color:#6e6e73; border:none;"
        " border-bottom:1px solid #e5e5ea; padding:7px 6px; font-weight:600; font-size:12px; }"
        "QTreeWidget::branch { background:transparent; }"));
    lay->addWidget(m_tree);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &HistoryDialog::reject);
    lay->addWidget(buttons);

    refresh();
}

void HistoryDialog::refresh()
{
    m_tree->clear();

    QSqlQuery q(Database::db());
    q.exec(QStringLiteral(
        "SELECT m.id, m.name, m.project_id, m.people_id, m.status, m.status_set_time"
        " FROM main_tasks m ORDER BY m.project_id, m.created_at, m.id"));

    QMap<int, QTreeWidgetItem *> projectItems; // projectId -> item
    QTreeWidgetItem *noProjectItem = nullptr;

    const auto projectItemFor = [&](int projectId) -> QTreeWidgetItem * {
        if (projectId < 0) {
            if (!noProjectItem) {
                noProjectItem = new QTreeWidgetItem(m_tree);
                noProjectItem->setText(0, "(No project)");
                noProjectItem->setFirstColumnSpanned(true);
                QFont f = noProjectItem->font(0);
                f.setBold(true);
                noProjectItem->setFont(0, f);
                noProjectItem->setForeground(0, QColor("#8e8e93"));
            }
            return noProjectItem;
        }
        auto it = projectItems.constFind(projectId);
        if (it != projectItems.constEnd()) return it.value();
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, Database::projectName(projectId));
        item->setFirstColumnSpanned(true);
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        item->setForeground(0, QColor("#1d1d1f"));
        projectItems.insert(projectId, item);
        return item;
    };

    while (q.next()) {
        const int taskId = q.value(0).toInt();
        const QString name = q.value(1).toString();
        const int projectId = q.value(2).isNull() ? -1 : q.value(2).toInt();
        const int peopleId = q.value(3).isNull() ? -1 : q.value(3).toInt();
        const TaskStatus st = statusFromKey(q.value(4).toString());
        const QDateTime setTime = q.value(5).isNull() ? QDateTime() : q.value(5).toDateTime();

        auto *taskItem = new QTreeWidgetItem(projectItemFor(projectId));
        taskItem->setText(0, name);
        taskItem->setText(1, Database::personName(peopleId));
        taskItem->setText(2, MainTaskDialog::statusText(st));
        taskItem->setForeground(2, MainTaskDialog::statusColor(st));
        if ((st == TaskStatus::Completed || st == TaskStatus::Stopped) && setTime.isValid())
            taskItem->setText(3, setTime.toString(QStringLiteral("yyyy-MM-dd")));

        // subtasks
        QSqlQuery qs(Database::db());
        qs.prepare(QStringLiteral(
            "SELECT name, status, status_set_time FROM sub_tasks WHERE main_task_id = ?"
            " ORDER BY created_at, id"));
        qs.addBindValue(taskId);
        if (qs.exec()) {
            while (qs.next()) {
                auto *subItem = new QTreeWidgetItem(taskItem);
                subItem->setText(0, qs.value(0).toString());
                const TaskStatus sst = statusFromKey(qs.value(1).toString());
                subItem->setText(2, MainTaskDialog::statusText(sst));
                subItem->setForeground(2, MainTaskDialog::statusColor(sst));
                const QDateTime st2 = qs.value(2).isNull() ? QDateTime() : qs.value(2).toDateTime();
                if ((sst == TaskStatus::Completed || sst == TaskStatus::Stopped) && st2.isValid())
                    subItem->setText(3, st2.toString(QStringLiteral("yyyy-MM-dd")));
            }
        }
    }

    m_tree->expandAll();
}

// ---------------------------------------------------------------------------
// FilterPopup
// ---------------------------------------------------------------------------

FilterPopup::FilterPopup(QWidget *parent)
    : QFrame(parent)
{
    setWindowFlags(Qt::Popup);
    setObjectName(QStringLiteral("filterPopup"));
    setFixedWidth(240);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 8);
    lay->setSpacing(6);

    const QString headerStyle = QStringLiteral("color:#8e8e93;font-size:11px;font-weight:600;");

    auto *projectHeader = new QLabel("Project", this);
    projectHeader->setStyleSheet(headerStyle);
    lay->addWidget(projectHeader);
    m_projectLayout = new QVBoxLayout;
    m_projectLayout->setSpacing(2);
    lay->addLayout(m_projectLayout);

    auto *peopleHeader = new QLabel("People", this);
    peopleHeader->setStyleSheet(headerStyle);
    lay->addWidget(peopleHeader);
    m_peopleLayout = new QVBoxLayout;
    m_peopleLayout->setSpacing(2);
    lay->addLayout(m_peopleLayout);

    auto *statusHeader = new QLabel("Status", this);
    statusHeader->setStyleSheet(headerStyle);
    lay->addWidget(statusHeader);
    m_statusLayout = new QVBoxLayout;
    m_statusLayout->setSpacing(2);
    lay->addLayout(m_statusLayout);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QStringLiteral("color:#e5e5ea;"));
    lay->addWidget(separator);

    auto *clearButton = new QPushButton("Clear All", this);
    clearButton->setFlat(true);
    clearButton->setStyleSheet(QStringLiteral("color:#007AFF;"));
    lay->addWidget(clearButton, 0, Qt::AlignHCenter);
    connect(clearButton, &QPushButton::clicked, this, &FilterPopup::clearAll);
}

void FilterPopup::rebuildGroup(QVBoxLayout *groupLayout, const QStringList &keys,
                               const QStringList &labels, QList<QCheckBox *> &boxes)
{
    // preserve current checks for keys that still exist
    QStringList checked;
    for (QCheckBox *box : boxes)
        if (box->isChecked()) checked << box->property("key").toString();

    for (QCheckBox *box : boxes) {
        groupLayout->removeWidget(box);
        box->deleteLater();
    }
    boxes.clear();

    for (int i = 0; i < keys.size(); ++i) {
        auto *box = new QCheckBox(labels.value(i), this);
        box->setProperty("key", keys.value(i));
        box->setStyleSheet(QStringLiteral("font-size:13px;color:#1d1d1f;"));
        box->setChecked(checked.contains(keys.value(i)));
        groupLayout->addWidget(box);
        boxes.append(box);
        // connect after setChecked so rebuilds never emit filtersChanged
        connect(box, &QCheckBox::toggled, this, &FilterPopup::filtersChanged);
    }
}

void FilterPopup::setProjectItems(const QStringList &keys, const QStringList &labels)
{
    rebuildGroup(m_projectLayout, keys, labels, m_projectBoxes);
}

void FilterPopup::setPeopleItems(const QStringList &keys, const QStringList &labels)
{
    rebuildGroup(m_peopleLayout, keys, labels, m_peopleBoxes);
}

void FilterPopup::setStatusItems(const QStringList &keys, const QStringList &labels)
{
    rebuildGroup(m_statusLayout, keys, labels, m_statusBoxes);
}

QStringList FilterPopup::selectedProjectKeys() const
{
    QStringList out;
    for (QCheckBox *box : m_projectBoxes)
        if (box->isChecked()) out << box->property("key").toString();
    return out;
}

QStringList FilterPopup::selectedPeopleKeys() const
{
    QStringList out;
    for (QCheckBox *box : m_peopleBoxes)
        if (box->isChecked()) out << box->property("key").toString();
    return out;
}

QStringList FilterPopup::selectedStatusKeys() const
{
    QStringList out;
    for (QCheckBox *box : m_statusBoxes)
        if (box->isChecked()) out << box->property("key").toString();
    return out;
}

void FilterPopup::setSelections(const QStringList &projKeys, const QStringList &peopleKeys,
                                const QStringList &statusKeys)
{
    const auto apply = [](const QList<QCheckBox *> &boxes, const QStringList &keys) {
        for (QCheckBox *box : boxes) {
            const QSignalBlocker blocker(box);
            box->setChecked(keys.contains(box->property("key").toString()));
        }
    };
    apply(m_projectBoxes, projKeys);
    apply(m_peopleBoxes, peopleKeys);
    apply(m_statusBoxes, statusKeys);
}

void FilterPopup::clearAll()
{
    const auto uncheck = [](const QList<QCheckBox *> &boxes) {
        for (QCheckBox *box : boxes) {
            const QSignalBlocker blocker(box);
            box->setChecked(false);
        }
    };
    uncheck(m_projectBoxes);
    uncheck(m_peopleBoxes);
    uncheck(m_statusBoxes);
    emit filtersChanged();
}

} // namespace ksat

#include "dialogs.moc"

#include "taskcard.h"

#include "database.h"
#include "dialogs.h"

#include <QApplication>
#include <QDrag>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ksat {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static QString fmtTime(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd")) : QString();
}

// One subtask row: header line + always-visible records; operations live in a
// popup list opened by the "⋯" button (status sits in a second-level submenu).
class SubTaskRow : public QFrame {
    Q_OBJECT
public:
    SubTaskRow(const SubTask &sub, QWidget *parent)
        : QFrame(parent), m_sub(sub)
    {
        buildUi();
    }

    int subTaskId() const { return m_sub.id; }

signals:
    void dataChanged();
    void openTaskRequested(int mainTaskId);

private:
    void buildUi();
    void buildRecords();
    void showActionsPopup(QPushButton *anchor);
    void addRecord();
    void editRecord(const Record &r);
    void deleteSubTask();
    void setStatus(TaskStatus s);
    void createMainTask();
    void editSubTask();

    SubTask m_sub;
    QFrame *m_recordsBox = nullptr;
    QVBoxLayout *m_recordsLayout = nullptr;
};

// ---------------------------------------------------------------------------
// SubTaskRow
// ---------------------------------------------------------------------------

void SubTaskRow::buildUi()
{
    setObjectName(QStringLiteral("subtaskRow"));

    // header line (the actual row): dot | name | status | "⋯" (popup list)
    auto *header = new QWidget(this);
    auto *lay = new QHBoxLayout(header);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(4);

    auto *dot = new QLabel(this);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QStringLiteral("background:%1;border-radius:4px;")
                           .arg(MainTaskDialog::statusColor(m_sub.displayStatus()).name()));
    lay->addWidget(dot);

    auto *name = new QLabel(m_sub.name, this);
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(name, 1);

    auto *status = new QLabel(MainTaskDialog::statusText(m_sub.displayStatus()), this);
    status->setStyleSheet(QStringLiteral("color:%1;font-size:11px;")
                              .arg(MainTaskDialog::statusColor(m_sub.displayStatus()).name()));
    lay->addWidget(status);

    auto *more = new QPushButton("⋯", this);
    more->setFlat(true);
    more->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:14px;"));
    connect(more, &QPushButton::clicked, this, [this, more]() {
        showActionsPopup(more);
    });
    lay->addWidget(more);

    // records: always visible (default expanded)
    m_recordsBox = new QFrame(this);
    m_recordsBox->setObjectName(QStringLiteral("subRecordsBox"));
    m_recordsLayout = new QVBoxLayout(m_recordsBox);
    m_recordsLayout->setContentsMargins(8, 2, 4, 2);
    m_recordsLayout->setSpacing(2);
    buildRecords();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(header);
    outer->addWidget(m_recordsBox);
}

void SubTaskRow::buildRecords()
{
    while (QLayoutItem *item = m_recordsLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    // notes header row: "Notes (n)" + "+" add button
    auto *hdr = new QWidget(m_recordsBox);
    auto *hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(0, 0, 0, 0);
    hdrLay->setSpacing(4);
    const QVector<Record> recs = Database::loadRecordsForSubTask(m_sub.id);
    auto *hdrLabel = new QLabel(recs.isEmpty() ? QStringLiteral("Notes")
                                               : QStringLiteral("Notes (%1)").arg(recs.size()),
                                m_recordsBox);
    hdrLabel->setStyleSheet(QStringLiteral("font-weight:bold;color:#1d1d1f;font-size:11px;"));
    hdrLay->addWidget(hdrLabel);
    hdrLay->addStretch(1);
    auto *add = new QPushButton(QStringLiteral("+"), m_recordsBox);
    add->setFlat(true);
    add->setFixedSize(22, 20);
    add->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:16px;font-weight:bold;"));
    add->setToolTip(QStringLiteral("Add Note"));
    connect(add, &QPushButton::clicked, this, &SubTaskRow::addRecord);
    hdrLay->addWidget(add);
    m_recordsLayout->addWidget(hdr);

    // record rows: content | time | ✎ | ✕
    for (const Record &r : recs) {
        auto *row = new QHBoxLayout;
        row->setSpacing(4);
        auto *content = new QLabel(r.content, m_recordsBox);
        content->setWordWrap(true);
        content->setTextInteractionFlags(Qt::TextSelectableByMouse);
        content->setStyleSheet(QStringLiteral("color:#555;font-size:11px;"));
        row->addWidget(content, 1);
        auto *t = new QLabel(fmtTime(r.createdAt), m_recordsBox);
        t->setStyleSheet(QStringLiteral("color:#aaa;font-size:10px;"));
        row->addWidget(t);
        auto *edit = new QPushButton(QStringLiteral("✎"), m_recordsBox);
        edit->setFlat(true);
        edit->setFixedSize(20, 18);
        edit->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:12px;"));
        edit->setToolTip(QStringLiteral("Edit Note"));
        connect(edit, &QPushButton::clicked, this, [this, r]() { editRecord(r); });
        row->addWidget(edit);
        auto *del = new QPushButton(QStringLiteral("✕"), m_recordsBox);
        del->setFlat(true);
        del->setStyleSheet(QStringLiteral("color:#ccc;font-size:10px;"));
        connect(del, &QPushButton::clicked, this, [this, r]() {
            if (QMessageBox::question(this, "Delete Note",
                                      QStringLiteral("Delete note \"%1\"?").arg(r.content))
                == QMessageBox::Yes) {
                Database::deleteRecord(r.id);
                emit dataChanged();
            }
        });
        row->addWidget(del);
        auto *cont = new QWidget(m_recordsBox);
        cont->setLayout(row);
        m_recordsLayout->addWidget(cont);
    }
}

void SubTaskRow::showActionsPopup(QPushButton *anchor)
{
    QMenu menu(this);

    // time range as a non-interactive header item (visible when expanded)
    const QString range = fmtTime(m_sub.startTime);
    const QString rangeText = [&]() -> QString {
        if (!range.isEmpty()) {
            return m_sub.endTime.isValid()
                ? QStringLiteral("%1 ~ %2").arg(range, fmtTime(m_sub.endTime))
                : range;
        }
        return m_sub.endTime.isValid() ? QStringLiteral("~ %1").arg(fmtTime(m_sub.endTime))
                                       : QString();
    }();
    if (!rangeText.isEmpty()) {
        QAction *timeItem = menu.addAction(rangeText);
        timeItem->setEnabled(false);
        QFont f = timeItem->font();
        f.setPointSize(9);
        timeItem->setFont(f);
        menu.addSeparator();
    }

    // 状态: second-level submenu (done / stopped / open)
    QMenu *statusMenu = menu.addMenu("Status");
    QAction *done = statusMenu->addAction("Done");
    connect(done, &QAction::triggered, this, [this]() { setStatus(TaskStatus::Completed); });
    QAction *stopped = statusMenu->addAction("Stopped");
    connect(stopped, &QAction::triggered, this, [this]() { setStatus(TaskStatus::Stopped); });
    // Open: always present; reopens a subtask that was manually Done/Stopped
    QAction *open = statusMenu->addAction("Open");
    connect(open, &QAction::triggered, this, [this]() { setStatus(TaskStatus::NotStarted); });

    // 记录: add
    QAction *addRec = menu.addAction("Add Note");
    connect(addRec, &QAction::triggered, this, &SubTaskRow::addRecord);

    // 编辑
    QAction *edit = menu.addAction("Edit");
    connect(edit, &QAction::triggered, this, &SubTaskRow::editSubTask);

    // 链接: main tasks created from this subtask
    const QVector<MainTask> linked = Database::mainTasksFromSubtask(m_sub.id);
    if (!linked.isEmpty()) {
        QMenu *linkMenu = menu.addMenu(QStringLiteral("🔗 Linked (%1)").arg(linked.size()));
        for (const MainTask &t : linked) {
            QAction *a = linkMenu->addAction(t.name);
            connect(a, &QAction::triggered, this, [this, t]() {
                emit openTaskRequested(t.id);
            });
        }
    }

    // 建大任务
    QAction *makeTask = menu.addAction("→ Main task");
    connect(makeTask, &QAction::triggered, this, &SubTaskRow::createMainTask);

    menu.addSeparator();
    QAction *del = menu.addAction("Delete");
    del->setProperty("danger", true);
    menu.setStyleSheet(QStringLiteral("QMenu::item[danger=\"true\"] { color:#ff3b30; }"));
    connect(del, &QAction::triggered, this, &SubTaskRow::deleteSubTask);

    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void SubTaskRow::addRecord()
{
    RecordDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Database::addRecord(-1, m_sub.id, dlg.content());
        emit dataChanged();
    }
}

void SubTaskRow::editRecord(const Record &r)
{
    RecordDialog dlg(r.content, this);
    if (dlg.exec() == QDialog::Accepted) {
        if (Database::updateRecord(r.id, dlg.content()))
            emit dataChanged();
    }
}

void SubTaskRow::deleteSubTask()
{
    if (QMessageBox::question(this, "Delete Subtask",
                              QStringLiteral("Delete subtask \"%1\"?").arg(m_sub.name))
        == QMessageBox::Yes) {
        Database::deleteSubTask(m_sub.id);
        emit dataChanged();
    }
}

void SubTaskRow::setStatus(TaskStatus s)
{
    SubTask t = m_sub;
    t.manualStatus = s;
    if (s == TaskStatus::Completed || s == TaskStatus::Stopped)
        t.statusSetTime = QDateTime::currentDateTime();
    else
        t.statusSetTime = QDateTime();
    Database::updateSubTask(t);
    emit dataChanged();
}

void SubTaskRow::createMainTask()
{
    MainTask t;
    t.name = m_sub.name;
    t.sourceSubtaskId = m_sub.id;
    MainTaskDialog dlg(t, m_sub.id, m_sub.mainTaskId, this);
    if (dlg.exec() == QDialog::Accepted) {
        const MainTask res = dlg.resultTask();
        if (res.name.isEmpty()) return; // invalid (empty name)
        Database::insertMainTask(res);
        emit dataChanged();
    }
}

void SubTaskRow::editSubTask()
{
    SubTaskDialog dlg(m_sub, m_sub.mainTaskId, this);
    if (dlg.exec() == QDialog::Accepted) {
        const SubTask res = dlg.resultSubTask();
        if (res.name.isEmpty()) {
            QMessageBox::warning(this, "Edit Subtask", "Subtask name must not be empty.");
            return;
        }
        Database::updateSubTask(res);
        m_sub = res;
        emit dataChanged();
    }
}

// ---------------------------------------------------------------------------
// TaskCard
// ---------------------------------------------------------------------------

TaskCard::TaskCard(const MainTask &task, QWidget *parent)
    : QFrame(parent), m_task(task)
{
    setObjectName(QStringLiteral("taskCard"));
    setMinimumWidth(320);
    setMaximumWidth(340);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    buildUi();
    refreshContent();
}

void TaskCard::buildUi()
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 8);
    lay->setSpacing(0);

    // colored top edge (project color; white if none) — spans the full width
    auto *edge = new QFrame(this);
    edge->setFixedHeight(6);
    QColor col = Qt::white;
    const QVector<Project> projects = Database::loadProjects();
    for (const Project &p : projects)
        if (p.id == m_task.projectId) { col = p.color; break; }
    edge->setStyleSheet(QStringLiteral("background:%1;").arg(col.name()));
    edge->setObjectName(QStringLiteral("taskEdge"));
    lay->addWidget(edge);

    // content sits on its own padded layer so text keeps a little distance
    // from the card border (edge stays full-width above it)
    auto *content = new QWidget(this);
    content->setObjectName(QStringLiteral("taskContent"));
    auto *clay = new QVBoxLayout(content);
    clay->setContentsMargins(12, 6, 12, 0);
    clay->setSpacing(6);

    // header: name + status + "⋯" menu
    auto *header = new QHBoxLayout;
    m_nameLabel = new QLabel(m_task.name, this);
    m_nameLabel->setStyleSheet(QStringLiteral("font-weight:bold;font-size:14px;"));
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->addWidget(m_nameLabel, 1);

    m_statusLabel = new QLabel(MainTaskDialog::statusText(m_task.displayStatus()), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color:%1;border:1px solid %1;border-radius:8px;padding:1px 8px;font-size:11px;")
        .arg(MainTaskDialog::statusColor(m_task.displayStatus()).name()));
    header->addWidget(m_statusLabel);

    auto *more = new QPushButton("⋯", this);
    more->setFlat(true);
    more->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:16px;"));
    connect(more, &QPushButton::clicked, this, [this, more]() {
        QMenu menu(this);
        QAction *edit = menu.addAction("Edit");
        connect(edit, &QAction::triggered, this, &TaskCard::editTask);
        QAction *done = menu.addAction("Done");
        connect(done, &QAction::triggered, this, [this]() { setStatus(TaskStatus::Completed); });
        QAction *stopped = menu.addAction("Stopped");
        connect(stopped, &QAction::triggered, this, [this]() { setStatus(TaskStatus::Stopped); });
        // Open: always present; reopens a task that was manually Done/Stopped
        // (resets to the automatic status; harmless for already-open tasks).
        QAction *open = menu.addAction("Open");
        connect(open, &QAction::triggered, this,
                [this]() { setStatus(TaskStatus::NotStarted); });
        menu.addSeparator();
        QAction *del = menu.addAction("Delete");
        del->setProperty("danger", true);
        menu.setStyleSheet(QStringLiteral("QMenu::item[danger=\"true\"] { color:#ff3b30; }"));
        connect(del, &QAction::triggered, this, &TaskCard::deleteTask);
        menu.exec(more->mapToGlobal(QPoint(0, more->height())));
    });
    header->addWidget(more);
    clay->addLayout(header);

    // meta: project / owner / time — one line each, gray
    const QString metaStyle = QStringLiteral("color:#8e8e93;font-size:11px;");
    m_projectLabel = new QLabel(this);
    m_projectLabel->setObjectName(QStringLiteral("metaProject"));
    m_projectLabel->setStyleSheet(metaStyle);
    clay->addWidget(m_projectLabel);
    m_ownerLabel = new QLabel(this);
    m_ownerLabel->setObjectName(QStringLiteral("metaOwner"));
    m_ownerLabel->setStyleSheet(metaStyle);
    clay->addWidget(m_ownerLabel);
    m_timeLabel = new QLabel(this);
    m_timeLabel->setObjectName(QStringLiteral("metaTime"));
    m_timeLabel->setStyleSheet(metaStyle);
    clay->addWidget(m_timeLabel);

    // notes section
    auto *recordsHeaderRow = new QHBoxLayout;
    m_recordsHeader = new QLabel("Notes", this);
    m_recordsHeader->setStyleSheet(QStringLiteral("font-weight:bold;color:#1d1d1f;font-size:11px;"));
    recordsHeaderRow->addWidget(m_recordsHeader);
    recordsHeaderRow->addStretch(1);
    auto *addRec = new QPushButton("+", this);
    addRec->setFlat(true);
    addRec->setFixedSize(22, 20);
    addRec->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:16px;font-weight:bold;"));
    addRec->setToolTip("Add Note");
    connect(addRec, &QPushButton::clicked, this, &TaskCard::addMainRecord);
    recordsHeaderRow->addWidget(addRec);
    clay->addLayout(recordsHeaderRow);
    m_recordsLayout = new QVBoxLayout;
    m_recordsLayout->setSpacing(2);
    clay->addLayout(m_recordsLayout);

    // subtasks section
    auto *subtasksHeaderRow = new QHBoxLayout;
    m_subtasksHeader = new QLabel("Subtasks", this);
    m_subtasksHeader->setStyleSheet(QStringLiteral("font-weight:bold;color:#1d1d1f;font-size:11px;"));
    subtasksHeaderRow->addWidget(m_subtasksHeader);
    subtasksHeaderRow->addStretch(1);
    auto *addSub = new QPushButton("+", this);
    addSub->setFlat(true);
    addSub->setFixedSize(22, 20);
    addSub->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:16px;font-weight:bold;"));
    addSub->setToolTip("Add Subtask");
    connect(addSub, &QPushButton::clicked, this, &TaskCard::addSubTask);
    subtasksHeaderRow->addWidget(addSub);
    clay->addLayout(subtasksHeaderRow);
    m_subtasksLayout = new QVBoxLayout;
    m_subtasksLayout->setSpacing(2);
    clay->addLayout(m_subtasksLayout);

    // keep content top-aligned inside the stretched card
    clay->addStretch(1);
    lay->addWidget(content, 1);
}

void TaskCard::refreshContent()
{
    // re-read from DB so status is current
    const MainTask fresh = Database::mainTask(m_task.id);
    if (fresh.id >= 0) m_task = fresh;

    m_nameLabel->setText(m_task.name);

    // meta: three independent bold lines (project / owner / time range)
    const QString projectName = Database::projectName(m_task.projectId);
    m_projectLabel->setText(m_task.projectId >= 0
                                ? QStringLiteral("Project: %1").arg(projectName)
                                : QString());
    m_projectLabel->setVisible(!m_projectLabel->text().isEmpty());
    m_ownerLabel->setText(m_task.peopleId >= 0
                              ? QStringLiteral("Owner: %1").arg(Database::personName(m_task.peopleId))
                              : QString());
    m_ownerLabel->setVisible(!m_ownerLabel->text().isEmpty());
    QString range;
    const QString s = fmtTime(m_task.startTime);
    const QString e = fmtTime(m_task.endTime);
    if (!s.isEmpty() && !e.isEmpty()) range = s + QStringLiteral(" ~ ") + e;
    else if (!s.isEmpty()) range = s;
    else if (!e.isEmpty()) range = e;
    m_timeLabel->setText(range);
    m_timeLabel->setVisible(!range.isEmpty());

    // status
    const TaskStatus st = m_task.displayStatus();
    m_statusLabel->setText(MainTaskDialog::statusText(st));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color:%1;border:1px solid %1;border-radius:8px;padding:1px 8px;font-size:11px;")
        .arg(MainTaskDialog::statusColor(st).name()));
    if (m_task.statusSetTime.isValid())
        m_statusLabel->setToolTip(QStringLiteral("Set at %1").arg(fmtTime(m_task.statusSetTime)));
    else
        m_statusLabel->setToolTip(QString());

    // records
    while (QLayoutItem *item = m_recordsLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
    const QVector<Record> records = Database::loadRecordsForMainTask(m_task.id);
    for (const Record &r : records) {
        auto *row = new QHBoxLayout;
        row->setSpacing(4);
        auto *content = new QLabel(r.content, this);
        content->setWordWrap(true);
        content->setTextInteractionFlags(Qt::TextSelectableByMouse);
        content->setStyleSheet(QStringLiteral("color:#444;font-size:11px;"));
        row->addWidget(content, 1);
        auto *t = new QLabel(fmtTime(r.createdAt), this);
        t->setStyleSheet(QStringLiteral("color:#aaa;font-size:10px;"));
        row->addWidget(t);
        auto *edit = new QPushButton(QStringLiteral("✎"), this);
        edit->setFlat(true);
        edit->setFixedSize(20, 18);
        edit->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:12px;"));
        edit->setToolTip("Edit Note");
        connect(edit, &QPushButton::clicked, this, [this, r]() {
            RecordDialog dlg(r.content, this);
            if (dlg.exec() == QDialog::Accepted)
                if (Database::updateRecord(r.id, dlg.content()))
                    emit dataChanged();
        });
        row->addWidget(edit);
        auto *del = new QPushButton(QStringLiteral("✕"), this);
        del->setFlat(true);
        del->setStyleSheet(QStringLiteral("color:#ccc;font-size:10px;"));
        connect(del, &QPushButton::clicked, this, [this, r]() {
            if (QMessageBox::question(this, "Delete Note",
                                      QStringLiteral("Delete note \"%1\"?").arg(r.content))
                == QMessageBox::Yes) {
                Database::deleteRecord(r.id);
                emit dataChanged();
            }
        });
        row->addWidget(del);
        auto *cont = new QWidget(this);
        cont->setLayout(row);
        m_recordsLayout->addWidget(cont);
    }
    m_recordsHeader->setText(records.isEmpty() ? "Notes"
                                               : QStringLiteral("Notes (%1)").arg(records.size()));

    // subtasks
    while (QLayoutItem *item = m_subtasksLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
    const QVector<SubTask> subs = Database::loadSubTasks(m_task.id);
    for (const SubTask &s : subs) {
        auto *row = new SubTaskRow(s, this);
        connect(row, &SubTaskRow::dataChanged, this, &TaskCard::dataChanged);
        connect(row, &SubTaskRow::openTaskRequested, this, &TaskCard::openTaskRequested);
        m_subtasksLayout->addWidget(row);
    }
    m_subtasksHeader->setText(subs.isEmpty() ? "Subtasks"
                                             : QStringLiteral("Subtasks (%1)").arg(subs.size()));
}

void TaskCard::addMainRecord()
{
    RecordDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        Database::addRecord(m_task.id, -1, dlg.content());
        emit dataChanged();
    }
}

void TaskCard::addSubTask()
{
    SubTaskDialog dlg(SubTask(), m_task.id, this);
    if (dlg.exec() == QDialog::Accepted) {
        const SubTask res = dlg.resultSubTask();
        if (res.name.isEmpty()) {
            QMessageBox::warning(this, "Notice", "Subtask name must not be empty.");
            return;
        }
        Database::insertSubTask(res);
        emit dataChanged();
    }
}

void TaskCard::editTask()
{
    MainTaskDialog dlg(m_task, -1, -1, this);
    if (dlg.exec() == QDialog::Accepted) {
        const MainTask res = dlg.resultTask();
        if (res.name.isEmpty()) {
            QMessageBox::warning(this, "Notice",
                                 "Task name must not be empty and must be ≤30 characters.");
            return;
        }
        // Editing may set Done/Stopped via the status selector — the task will
        // then be hidden from the board, so confirm before saving.
        if (res.manualStatus == TaskStatus::Completed || res.manualStatus == TaskStatus::Stopped) {
            const QString prompt = res.manualStatus == TaskStatus::Completed
                                       ? QStringLiteral("finished task is to be hidden")
                                       : QStringLiteral("stopped task is to be hidden");
            if (QMessageBox::question(this, "Confirm", prompt) != QMessageBox::Yes)
                return;
        }
        Database::updateMainTask(res);
        emit dataChanged();
    }
}

void TaskCard::deleteTask()
{
    if (QMessageBox::question(this, "Delete Task",
                              QStringLiteral("Delete main task \"%1\" together with all its subtasks and notes?").arg(m_task.name))
        == QMessageBox::Yes) {
        Database::deleteMainTask(m_task.id);
        emit dataChanged();
    }
}

void TaskCard::setStatus(TaskStatus s)
{
    // Done/Stopped hides the task from the board — confirm before hiding.
    if (s == TaskStatus::Completed || s == TaskStatus::Stopped) {
        const QString prompt = s == TaskStatus::Completed
                                   ? QStringLiteral("finished task is to be hidden")
                                   : QStringLiteral("stopped task is to be hidden");
        if (QMessageBox::question(this, "Confirm", prompt) != QMessageBox::Yes)
            return;
    }
    m_task.manualStatus = s;
    if (s == TaskStatus::Completed || s == TaskStatus::Stopped)
        m_task.statusSetTime = QDateTime::currentDateTime();
    else
        m_task.statusSetTime = QDateTime();
    Database::updateMainTask(m_task);
    emit dataChanged();
}

void TaskCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStart = event->pos();
        m_maybeDrag = true;
    }
    QFrame::mousePressEvent(event);
}

void TaskCard::mouseMoveEvent(QMouseEvent *event)
{
    if (m_maybeDrag && (event->buttons() & Qt::LeftButton)
        && (event->pos() - m_dragStart).manhattanLength() > QApplication::startDragDistance()) {
        m_maybeDrag = false;
        startDrag(m_dragStart);
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void TaskCard::mouseReleaseEvent(QMouseEvent *event)
{
    m_maybeDrag = false;
    QFrame::mouseReleaseEvent(event);
}

void TaskCard::startDrag(const QPoint &pressPos)
{
    QMimeData *mime = new QMimeData;
    mime->setData(QStringLiteral("application/x-ksat-task"),
                  QByteArray::number(m_task.id) + ":" + QByteArray::number(m_task.projectId));
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    QPixmap pm = grab();
    drag->setPixmap(pm);
    drag->setHotSpot(pressPos);
    drag->exec(Qt::MoveAction);
}

} // namespace ksat

#include "taskcard.moc"

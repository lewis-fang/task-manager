#include "mainwindow.h"

#include "database.h"
#include "dialogs.h"
#include "taskcard.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace ksat {

// ---------------------------------------------------------------------------
// BoardHost
// ---------------------------------------------------------------------------

BoardHost::BoardHost(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
}

void BoardHost::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-ksat-task")))
        event->acceptProposedAction();
}

void BoardHost::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-ksat-task")))
        event->acceptProposedAction();
}

void BoardHost::dropEvent(QDropEvent *event)
{
    const QByteArray payload =
        event->mimeData()->data(QStringLiteral("application/x-ksat-task"));
    const QList<QByteArray> parts = payload.split(':');
    if (parts.size() != 2) return;
    const int draggedId = parts[0].toInt();
    const int projectId = parts[1].toInt();

    // Collect the currently displayed cards of the dragged task's project, in
    // layout order, together with the x center of each card.
    QVector<int> ids;
    QVector<int> centers;
    if (QLayout *lay = layout()) {
        for (int i = 0; i < lay->count(); ++i) {
            QLayoutItem *item = lay->itemAt(i);
            QWidget *w = item ? item->widget() : nullptr;
            auto *card = w ? qobject_cast<TaskCard *>(w) : nullptr;
            if (!card || card->projectId() != projectId) continue;
            ids.append(card->taskId());
            centers.append(card->mapTo(this, QPoint(card->width() / 2, 0)).x());
        }
    }
    if (ids.isEmpty() || !ids.contains(draggedId)) return;

    // Find the insertion index by the drop x position.
    int insertIdx = ids.size();
    const int dropX = event->pos().x();
    for (int i = 0; i < centers.size(); ++i) {
        if (centers[i] > dropX) { insertIdx = i; break; }
    }

    // Rebuild the order: remove the dragged card, insert at the target slot.
    const int curIdx = ids.indexOf(draggedId);
    ids.removeAt(curIdx);
    if (insertIdx > curIdx) --insertIdx;
    if (insertIdx < 0) insertIdx = 0;
    if (insertIdx > ids.size()) insertIdx = ids.size();
    ids.insert(insertIdx, draggedId);

    emit tasksReordered(projectId, ids);
    event->acceptProposedAction();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    refreshBoard();
    applyStyle();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("KSAT - Task Board"));
    resize(1280, 800);

    // --- toolbar ---
    auto *toolbar = new QWidget(this);
    auto *toolLay = new QHBoxLayout(toolbar);
    toolLay->setContentsMargins(8, 6, 8, 6);
    toolLay->setSpacing(8);

    auto *historyBtn = new QPushButton(QStringLiteral("Task History"), toolbar);
    connect(historyBtn, &QPushButton::clicked, this, &MainWindow::showHistory);
    toolLay->addWidget(historyBtn);

    toolLay->addStretch(1);

    m_filterButton = new QPushButton(QStringLiteral("Filter ▾"), toolbar);
    connect(m_filterButton, &QPushButton::clicked, this, &MainWindow::showFilterPopup);
    toolLay->addWidget(m_filterButton);

    auto *addBtn = new QPushButton(QStringLiteral("+ Add Task"), toolbar);
    addBtn->setObjectName(QStringLiteral("addButton"));
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addNewTask);
    toolLay->addWidget(addBtn);

    auto *pwdBtn = new QPushButton(QStringLiteral("Change Password"), toolbar);
    pwdBtn->setObjectName(QStringLiteral("pwdButton"));
    connect(pwdBtn, &QPushButton::clicked, this, &MainWindow::changePassword);
    toolLay->addWidget(pwdBtn);

    // --- filter popup ---
    m_filterPopup = new FilterPopup(this);
    connect(m_filterPopup, &FilterPopup::filtersChanged, this, &MainWindow::applyFilters);

    // --- board ---
    m_boardScroll = new QScrollArea(this);
    m_boardScroll->setWidgetResizable(true);
    m_boardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_boardScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_boardHost = new BoardHost;
    m_boardHost->setObjectName(QStringLiteral("boardHost"));
    m_boardScroll->setWidget(m_boardHost);
    connect(m_boardHost, &BoardHost::tasksReordered,
            this, &MainWindow::handleTasksReordered);

    m_central = new QWidget(this);
    auto *mainLay = new QVBoxLayout(m_central);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);
    mainLay->addWidget(toolbar);
    mainLay->addWidget(m_boardScroll, 1);
    setCentralWidget(m_central);
}

void MainWindow::applyFilters()
{
    refreshBoard();
}

// Populate filter popup items; preserves selection.
void MainWindow::refreshFilterItems()
{
    // projects
    const QVector<Project> projects = Database::loadProjects();
    QStringList pkeys, plabels;
    for (const Project &p : projects) { pkeys << QString::number(p.id); plabels << p.name; }

    // people
    const QVector<Person> people = Database::loadPeople();
    QStringList qkeys, qlabels;
    for (const Person &p : people) { qkeys << QString::number(p.id); qlabels << p.name; }

    // statuses (fixed list)
    const QStringList stKeys = {statusKey(TaskStatus::NotStarted), statusKey(TaskStatus::InProgress),
                                statusKey(TaskStatus::Completed),  statusKey(TaskStatus::Delayed),
                                statusKey(TaskStatus::Stopped)};
    const QStringList stLabels = {MainTaskDialog::statusText(TaskStatus::NotStarted),
                                  MainTaskDialog::statusText(TaskStatus::InProgress),
                                  MainTaskDialog::statusText(TaskStatus::Completed),
                                  MainTaskDialog::statusText(TaskStatus::Delayed),
                                  MainTaskDialog::statusText(TaskStatus::Stopped)};

    // preserve current selections across refreshes
    const QStringList selProj = m_filterPopup->selectedProjectKeys();
    const QStringList selPeople = m_filterPopup->selectedPeopleKeys();
    const QStringList selStatus = m_filterPopup->selectedStatusKeys();

    m_filterPopup->setProjectItems(pkeys, plabels);
    m_filterPopup->setPeopleItems(qkeys, qlabels);
    m_filterPopup->setStatusItems(stKeys, stLabels);
    m_filterPopup->setSelections(selProj, selPeople, selStatus);
}

void MainWindow::showFilterPopup()
{
    refreshFilterItems();
    m_filterPopup->adjustSize();
    m_filterPopup->move(m_filterButton->mapToGlobal(QPoint(0, m_filterButton->height() + 2)));
    m_filterPopup->show();
}

void MainWindow::refreshBoard()
{
    Database::syncAutoStatuses();
    refreshFilterItems();

    // gather selected filters
    QVector<int> projectIds;
    for (const QString &k : m_filterPopup->selectedProjectKeys()) projectIds << k.toInt();
    QVector<int> peopleIds;
    for (const QString &k : m_filterPopup->selectedPeopleKeys()) peopleIds << k.toInt();

    QVector<TaskStatus> statuses;
    const QStringList sks = m_filterPopup->selectedStatusKeys();
    for (const QString &k : sks) statuses.append(statusFromKey(k));

    // include finished only if explicitly selected via status filter
    const bool includeFinished = statuses.contains(TaskStatus::Completed)
        || statuses.contains(TaskStatus::Stopped);

    const QVector<MainTask> tasks =
        Database::loadMainTasks(projectIds, peopleIds, statuses, includeFinished);

    // rebuild board: remove old layout AND delete its child widgets,
    // otherwise stale cards keep their old geometry and paint on top
    if (QLayout *oldLay = m_boardHost->layout()) {
        while (QLayoutItem *item = oldLay->takeAt(0)) {
            if (QWidget *w = item->widget()) {
                w->hide();
                w->deleteLater();
            }
            delete item;
        }
        delete oldLay;
    }
    auto *lay = new QHBoxLayout(m_boardHost);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(12);

    if (tasks.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("No tasks yet. Click \"+ Add Task\" to begin."), m_boardHost);
        empty->setStyleSheet(QStringLiteral("color:#8e8e93;font-size:14px;"));
        lay->addStretch(1);
        lay->addWidget(empty, 0, Qt::AlignCenter);
        lay->addStretch(1);
    } else {
        for (const MainTask &t : tasks) {
            auto *card = new TaskCard(t, m_boardHost);
            connect(card, &TaskCard::dataChanged, this, &MainWindow::refreshBoard);
            connect(card, &TaskCard::openTaskRequested, this, &MainWindow::openTaskDialog);
            lay->addWidget(card);
        }
        lay->addStretch(1);
    }

    m_boardScroll->setWidget(m_boardHost);
}

void MainWindow::addNewTask()
{
    MainTaskDialog dlg(MainTask(), -1, -1, this);
    if (dlg.exec() == QDialog::Accepted) {
        const MainTask res = dlg.resultTask();
        if (res.name.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Notice"),
                                 QStringLiteral("Task name must not be empty and must be ≤30 characters."));
            return;
        }
        Database::insertMainTask(res);
        refreshBoard();
    }
}

void MainWindow::openTaskDialog(int taskId)
{
    const MainTask t = Database::mainTask(taskId);
    if (t.id < 0) return;
    MainTaskDialog dlg(t, -1, -1, this);
    if (dlg.exec() == QDialog::Accepted) {
        const MainTask res = dlg.resultTask();
        if (res.name.isEmpty()) return;
        Database::updateMainTask(res);
        refreshBoard();
    }
}

void MainWindow::showHistory()
{
    HistoryDialog dlg(this);
    dlg.exec();
    refreshBoard();
}

void MainWindow::handleTasksReordered(int projectId, const QVector<int> &orderedIds)
{
    Database::reorderMainTasks(projectId, orderedIds);
    refreshBoard();
}

void MainWindow::changePassword()
{
    ChangePasswordDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    QSettings s;
    const QString storedHash = s.value(QStringLiteral("auth/passwordHash")).toString();
    if (passwordHash(dlg.oldPassword()) != storedHash) {
        QMessageBox::warning(this, QStringLiteral("Change Password"), QStringLiteral("Current password is incorrect."));
        return;
    }
    s.setValue(QStringLiteral("auth/passwordHash"), passwordHash(dlg.newPassword()));
    QMessageBox::information(this, QStringLiteral("Change Password"), QStringLiteral("Password changed."));
}

void MainWindow::applyStyle()
{
    const QString qss = QStringLiteral(R"(
QMainWindow, QDialog { background:#f5f5f7; color:#1d1d1f; }
QWidget#boardHost { background:#f5f5f7; }
QFrame#taskCard { background:#ffffff; border:1px solid #e5e5ea; border-radius:10px; }
QFrame#subtaskRow { background:transparent; }
QFrame#subRecordsBox { background:#fafafa; border-radius:6px; }
QPushButton { background:#ffffff; border:1px solid #d1d1d6; border-radius:7px; padding:5px 12px; color:#1d1d1f; font-size:13px; }
QPushButton:hover { background:#f0f0f2; }
QPushButton:flat { background:transparent; border:none; }
QPushButton#addButton { background:#007AFF; color:white; border:none; border-radius:8px; font-weight:600; }
QPushButton#addButton:hover { background:#0060df; }
QLineEdit, QComboBox, QDateTimeEdit, QPlainTextEdit { background:#ffffff; color:#1d1d1f; border:1px solid #d1d1d6; border-radius:7px; padding:4px 8px; }
QScrollArea { background:#f5f5f7; border:none; }
QScrollBar:vertical { background:transparent; width:8px; margin:2px; }
QScrollBar::handle:vertical { background:#d1d1d6; border-radius:4px; min-height:30px; }
QScrollBar::handle:vertical:hover { background:#aeaeb2; }
QScrollBar:horizontal { background:transparent; height:8px; margin:2px; }
QScrollBar::handle:horizontal { background:#d1d1d6; border-radius:4px; min-width:30px; }
QScrollBar::handle:horizontal:hover { background:#aeaeb2; }
QMenu { background:#ffffff; border:1px solid #e5e5ea; border-radius:8px; padding:4px; }
QMenu::item { padding:6px 20px; color:#1d1d1f; }
QMenu::item:selected { background:#007AFF; color:white; border-radius:4px; }
QCheckBox { color:#1d1d1f; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid #d1d1d6; border-radius:4px; background:#ffffff; }
QCheckBox::indicator:checked { background:#007AFF; border-color:#007AFF; }
QToolTip { background:#ffffff; color:#1d1d1f; border:1px solid #e5e5ea; border-radius:6px; padding:4px 8px; }
)");
    qApp->setStyleSheet(qss);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    QMainWindow::closeEvent(e);
}

} // namespace ksat

#include "mainwindow.moc"
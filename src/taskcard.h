#pragma once

#include "models.h"

#include <QFrame>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QMouseEvent;
class QPoint;

namespace ksat {

// A kanban card for one main task: colored top edge (project color), name,
// meta info, records (one line each), subtask rows (one line each).
class TaskCard : public QFrame {
    Q_OBJECT
public:
    TaskCard(const MainTask &task, QWidget *parent = nullptr);

    int taskId() const { return m_task.id; }
    int projectId() const { return m_task.projectId; }

signals:
    void dataChanged();          // any mutation inside the card
    void openTaskRequested(int taskId);  // open an existing main task dialog
    void navigateToTask(int taskId);     // scroll/select a task on the board

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void buildUi();
    void refreshContent();
    void addMainRecord();
    void addSubTask();
    void editTask();
    void deleteTask();
    void setStatus(TaskStatus s);
    void startDrag(const QPoint &pressPos);

    MainTask m_task;
    QPoint m_dragStart;
    bool m_maybeDrag = false;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_projectLabel = nullptr;  // one meta line each (project / owner / time)
    QLabel *m_ownerLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QVBoxLayout *m_recordsLayout = nullptr;
    QVBoxLayout *m_subtasksLayout = nullptr;
    QLabel *m_recordsHeader = nullptr;
    QLabel *m_subtasksHeader = nullptr;
};

} // namespace ksat

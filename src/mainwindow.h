#pragma once

#include "models.h"

#include <QMainWindow>

class QDragEnterEvent;
class QDropEvent;
class QPushButton;
class QScrollArea;
class QWidget;

namespace ksat {

class FilterPopup;
class TaskCard;

// The board surface. Accepts task-card drags and resolves them into a new
// display order for the dragged task's project group.
class BoardHost : public QWidget {
    Q_OBJECT
public:
    explicit BoardHost(QWidget *parent = nullptr);

signals:
    // Emitted after a drop reordered the tasks of one project.
    void tasksReordered(int projectId, const QVector<int> &orderedIds);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Reload all data and rebuild the board.
    void refreshBoard();

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void buildUi();
    void addNewTask();
    void showHistory();
    void applyFilters();
    void refreshFilterItems();
    void openTaskDialog(int taskId);
    void applyStyle();
    void changePassword();
    void handleTasksReordered(int projectId, const QVector<int> &orderedIds);

private slots:
    void showFilterPopup();

private:
    QWidget *m_central = nullptr;
    BoardHost *m_boardHost = nullptr;
    QScrollArea *m_boardScroll = nullptr;
    QPushButton *m_filterButton = nullptr;
    FilterPopup *m_filterPopup = nullptr;
};

} // namespace ksat
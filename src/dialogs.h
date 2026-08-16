#pragma once

#include "models.h"

#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QStringList>

class QCheckBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;
class QVBoxLayout;

namespace ksat {

// A row widget: checkbox label + datetime edit (unchecked = no time).
class DateTimeEditRow : public QWidget {
    Q_OBJECT
public:
    explicit DateTimeEditRow(const QString &label, QWidget *parent = nullptr);
    QDateTime dateTime() const;
    void setDateTime(const QDateTime &dt);

private:
    QCheckBox *m_check = nullptr;
    QDateTimeEdit *m_edit = nullptr;
};

// Dialog to create or edit a main task.
class MainTaskDialog : public QDialog {
    Q_OBJECT
public:
    // If task.id < 0 -> create mode. If sourceSubtaskId >= 0 the new task is
    // created FROM that subtask (project inherited from the subtask's parent).
    MainTaskDialog(const MainTask &task, int sourceSubtaskId, int sourceParentTaskId,
                   QWidget *parent = nullptr);

    MainTask resultTask() const;

    void accept() override;

    static QString statusText(TaskStatus s); // English status text
    static QColor statusColor(TaskStatus s);

private:
    void buildUi();
    void loadCombos();

    MainTask m_task;
    int m_sourceSubtaskId = -1;
    int m_sourceParentTaskId = -1;

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_projectCombo = nullptr;
    QComboBox *m_peopleCombo = nullptr;
    DateTimeEditRow *m_startEdit = nullptr;
    DateTimeEditRow *m_endEdit = nullptr;
    QComboBox *m_statusCombo = nullptr;
    QLabel *m_sourceLabel = nullptr;
    QLabel *m_statusInfo = nullptr;
};

// Dialog to create or edit a subtask.
class SubTaskDialog : public QDialog {
    Q_OBJECT
public:
    SubTaskDialog(const SubTask &sub, int mainTaskId, QWidget *parent = nullptr);
    SubTask resultSubTask() const;

    void accept() override;

private:
    void buildUi();

    SubTask m_sub;
    QLineEdit *m_nameEdit = nullptr;
    DateTimeEditRow *m_startEdit = nullptr;
    DateTimeEditRow *m_endEdit = nullptr;
    QComboBox *m_statusCombo = nullptr;
    QLabel *m_statusInfo = nullptr;
};

// Dialog for entering record content. With initialContent it becomes the
// "Edit Note" mode (pre-filled); without it, "Add Note".
class RecordDialog : public QDialog {
    Q_OBJECT
public:
    explicit RecordDialog(QWidget *parent = nullptr);
    RecordDialog(const QString &initialContent, QWidget *parent = nullptr);
    QString content() const;

private:
    QPlainTextEdit *m_edit = nullptr;
};

// History tree: project -> task (-> subtasks), with people/status/set-time.
class HistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit HistoryDialog(QWidget *parent = nullptr);
    void refresh();

private:
    QTreeWidget *m_tree = nullptr;
};

// Salted SHA-256 hash (hex) of the login password. The salt is a fixed
// per-app string; sufficient for a local privacy lock.
QString passwordHash(const QString &password);

// Password entry shown before the main window.
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    QString password() const;

private:
    QLineEdit *m_pass = nullptr;
};

// Dialog to change the login password (current + new + confirm).
class ChangePasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChangePasswordDialog(QWidget *parent = nullptr);
    QString oldPassword() const;
    QString newPassword() const;

private:
    QLineEdit *m_old = nullptr;
    QLineEdit *m_new = nullptr;
    QLineEdit *m_confirm = nullptr;
};

// Popup filter panel: three checkbox groups (project / people / status).
// Keys are opaque strings; labels are what the user sees.
class FilterPopup : public QFrame {
    Q_OBJECT
public:
    explicit FilterPopup(QWidget *parent = nullptr);

    void setProjectItems(const QStringList &keys, const QStringList &labels);
    void setPeopleItems(const QStringList &keys, const QStringList &labels);
    void setStatusItems(const QStringList &keys, const QStringList &labels);

    QStringList selectedProjectKeys() const;
    QStringList selectedPeopleKeys() const;
    QStringList selectedStatusKeys() const;

    void setSelections(const QStringList &projKeys, const QStringList &peopleKeys,
                       const QStringList &statusKeys);
    void clearAll();

signals:
    void filtersChanged();

private:
    void rebuildGroup(QVBoxLayout *groupLayout, const QStringList &keys,
                      const QStringList &labels, QList<QCheckBox *> &boxes);

    QVBoxLayout *m_projectLayout = nullptr;
    QVBoxLayout *m_peopleLayout = nullptr;
    QVBoxLayout *m_statusLayout = nullptr;
    QList<QCheckBox *> m_projectBoxes;
    QList<QCheckBox *> m_peopleBoxes;
    QList<QCheckBox *> m_statusBoxes;
};

} // namespace ksat

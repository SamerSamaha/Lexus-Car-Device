#pragma once

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/app_process_manager.h"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include <vector>

namespace lexus_head_unit {

// The registry entries as list rows for the QML grid.
class AppListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount CONSTANT)

public:
    enum Role {
        AppIdRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
    };

    explicit AppListModel(std::vector<AppEntry> entries, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] const AppEntry* entryAt(int row) const;
    [[nodiscard]] const AppEntry* entryById(const QString& appId) const;

private:
    std::vector<AppEntry> m_entries;
};

// What the hub's QML binds to (DN-021). Owns the 100 ms timer that drives
// AppProcessManager::poll(); every call happens on the UI thread.
class HubViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(lexus_head_unit::AppListModel* apps READ apps CONSTANT)
    Q_PROPERTY(bool appRunning READ appRunning NOTIFY stateChanged)
    Q_PROPERTY(QString foregroundAppName READ foregroundAppName NOTIFY stateChanged)
    Q_PROPERTY(QString lastExitText READ lastExitText NOTIFY stateChanged)
    Q_PROPERTY(QString registryErrorText READ registryErrorText CONSTANT)

public:
    HubViewModel(AppProcessManager& manager,
                 std::vector<AppEntry> entries,
                 const QStringList& registryErrors,
                 QObject* parent = nullptr);

    [[nodiscard]] AppListModel* apps();
    [[nodiscard]] bool appRunning() const;
    [[nodiscard]] QString foregroundAppName() const;
    [[nodiscard]] QString lastExitText() const;
    [[nodiscard]] QString registryErrorText() const;
    [[nodiscard]] const AppProcessManager& manager() const;

    Q_INVOKABLE bool launch(int row);
    Q_INVOKABLE bool requestReturn();
    // Result text of a launch by registry id, for the control socket: "started", "unknown app",
    // or a LaunchResult name.
    QString launchById(const QString& appId);
    void startPolling(int intervalMilliseconds);
    void poll();

signals:
    void stateChanged();

private:
    QString launchEntry(const AppEntry& entry);
    void refresh();
    QString nameOf(const std::string& appId) const;

    AppProcessManager* m_manager;
    AppListModel m_apps;
    QString m_registryErrorText;
    QTimer m_timer;
    bool m_appRunning = false;
    QString m_foregroundAppName;
    QString m_lastExitText;
    QString m_lastExitTextSeen;
};

} // namespace lexus_head_unit

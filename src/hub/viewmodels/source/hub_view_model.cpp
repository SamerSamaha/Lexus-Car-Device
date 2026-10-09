#include "lexus_head_unit/hub/hub_view_model.h"

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/app_process_manager.h"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Qt declares its macros, enumerations and QStringLiteral in internal headers; the public
// ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

AppListModel::AppListModel(std::vector<AppEntry> entries, QObject* parent)
    : QAbstractListModel(parent), m_entries(std::move(entries)) {}

int AppListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant AppListModel::data(const QModelIndex& index, int role) const {
    const AppEntry* entry = index.isValid() ? entryAt(index.row()) : nullptr;
    if (entry == nullptr) {
        return {};
    }
    switch (role) {
    case AppIdRole:
        return QString::fromStdString(entry->id);
    case NameRole:
    case Qt::DisplayRole:
        return QString::fromStdString(entry->name);
    case IconRole:
        return QString::fromStdString(entry->icon);
    default:
        return {};
    }
}

QHash<int, QByteArray> AppListModel::roleNames() const {
    return {{AppIdRole, "appId"}, {NameRole, "name"}, {IconRole, "icon"}};
}

const AppEntry* AppListModel::entryAt(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries.at(static_cast<std::size_t>(row));
}

const AppEntry* AppListModel::entryById(const QString& appId) const {
    const std::string wanted = appId.toStdString();
    for (const AppEntry& entry : m_entries) {
        if (entry.id == wanted) {
            return &entry;
        }
    }
    return nullptr;
}

HubViewModel::HubViewModel(AppProcessManager& manager,
                           std::vector<AppEntry> entries,
                           const QStringList& registryErrors,
                           QObject* parent)
    : QObject(parent),
      m_manager(&manager),
      m_apps(std::move(entries), this),
      m_registryErrorText(registryErrors.join(QStringLiteral("; "))) {
    m_manager->setExitListener([this](const AppExitReport& exitReport) {
        m_lastExitText = QString::fromStdString(
            describeExit(exitReport, nameOf(exitReport.appId).toStdString()));
    });
    connect(&m_timer, &QTimer::timeout, this, &HubViewModel::poll);
}

AppListModel* HubViewModel::apps() {
    return &m_apps;
}

bool HubViewModel::appRunning() const {
    return m_appRunning;
}

QString HubViewModel::foregroundAppName() const {
    return m_foregroundAppName;
}

QString HubViewModel::lastExitText() const {
    return m_lastExitText;
}

QString HubViewModel::registryErrorText() const {
    return m_registryErrorText;
}

const AppProcessManager& HubViewModel::manager() const {
    return *m_manager;
}

bool HubViewModel::launch(int row) {
    const AppEntry* entry = m_apps.entryAt(row);
    if (entry == nullptr) {
        return false;
    }
    return launchEntry(*entry) == QStringLiteral("started");
}

QString HubViewModel::launchById(const QString& appId) {
    const AppEntry* entry = m_apps.entryById(appId);
    if (entry == nullptr) {
        return QStringLiteral("unknown app");
    }
    return launchEntry(*entry);
}

QString HubViewModel::launchEntry(const AppEntry& entry) {
    const LaunchResult result = m_manager->launch(entry);
    refresh();
    return QString::fromUtf8(toString(result).data(), static_cast<int>(toString(result).size()));
}

bool HubViewModel::requestReturn() {
    const bool stopping = m_manager->requestStop();
    refresh();
    return stopping;
}

void HubViewModel::startPolling(int intervalMilliseconds) {
    m_timer.start(intervalMilliseconds);
}

void HubViewModel::poll() {
    m_manager->poll();
    refresh();
}

void HubViewModel::refresh() {
    const bool running = m_manager->runState() != AppRunState::Idle;
    const QString name = running ? nameOf(m_manager->foregroundAppId()) : QString();
    // The exit text is set by the listener during poll(); it is compared too, so that a new exit
    // with the same running state still notifies.
    const bool changed = running != m_appRunning || name != m_foregroundAppName ||
                         m_lastExitText != m_lastExitTextSeen;
    m_appRunning = running;
    m_foregroundAppName = name;
    m_lastExitTextSeen = m_lastExitText;
    if (changed) {
        emit stateChanged();
    }
}

QString HubViewModel::nameOf(const std::string& appId) const {
    const AppEntry* entry = m_apps.entryById(QString::fromStdString(appId));
    return entry == nullptr ? QString::fromStdString(appId) : QString::fromStdString(entry->name);
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)

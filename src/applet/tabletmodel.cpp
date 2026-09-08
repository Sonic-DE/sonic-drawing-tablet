/*
 *  SPDX-FileCopyrightText: 2025 SonicDE
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "tabletmodel.h"
#include "multidbuspendingcallwatcher.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>

#include <memory>

#include "wacomappletinterface.h"

Q_LOGGING_CATEGORY(LOG_WACOM_APPLET, "org.kde.plasma.wacomtablet")

static const QString s_wacomService = QStringLiteral("org.kde.Wacom");
static const QString s_wacomPath = QStringLiteral("/Tablet");

// Device names
static const QString s_stylus = QStringLiteral("stylus");
static const QString s_eraser = QStringLiteral("eraser");
static const QString s_touch = QStringLiteral("touch");

// Property keys
static const QString s_mode = QStringLiteral("Mode");
static const QString s_rotate = QStringLiteral("Rotate");
static const QString s_touchProp = QStringLiteral("Touch");

// Info key
static const QString s_tabletName = QStringLiteral("TabletName");

// Value literals
static const QString s_on = QStringLiteral("on");
static const QString s_absolute = QStringLiteral("absolute");

static QString sourceName(const QString& tabletId)
{
    return QStringLiteral("Tablet%1").arg(tabletId);
}

TabletModel::TabletModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_watcher = new QDBusServiceWatcher(s_wacomService, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString&, const QString&, const QString& newOwner) {
        if (newOwner.isEmpty()) {
            onServiceUnregistered();
        } else {
            onServiceRegistered();
        }
    });

    // Check if service is already running
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameHasOwner"));
    msg << s_wacomService;

    QDBusPendingCallWatcher* w = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    const int generation = m_generation;
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher* watcher) {
        QDBusPendingReply<bool> reply(*watcher);
        if (generation == m_generation && !reply.isError() && reply.value()) {
            onServiceRegistered();
        }
        watcher->deleteLater();
    });
}

TabletModel::~TabletModel() = default;

int TabletModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_tablets.size();
}

QVariant TabletModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tablets.size()) {
        return {};
    }
    const auto& entry = m_tablets.at(index.row());
    switch (role) {
    case IdRole:
        return entry.id;
    case NameRole:
        return entry.name;
    case ProfilesRole:
        return entry.profiles;
    case CurrentProfileRole:
        return entry.currentProfile;
    case StylusModeRole:
        return entry.stylusMode;
    case HasTouchRole:
        return entry.hasTouch;
    case TouchRole:
        return entry.touch;
    case DataEngineSourceRole:
        return sourceName(entry.id);
    }
    return {};
}

QHash<int, QByteArray> TabletModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {ProfilesRole, "profiles"},
        {CurrentProfileRole, "currentProfile"},
        {StylusModeRole, "stylusMode"},
        {HasTouchRole, "hasTouch"},
        {TouchRole, "touch"},
        {DataEngineSourceRole, "DataEngineSource"},
    };
}

int TabletModel::count() const
{
    return m_tablets.size();
}

bool TabletModel::serviceAvailable() const
{
    return m_serviceAvailable;
}

qulonglong TabletModel::revision() const
{
    return m_revision;
}

QStringList TabletModel::sources() const
{
    QStringList result{QStringLiteral("wacomtablet")};
    result.reserve(m_tablets.size() + 1);
    for (const TabletEntry& entry : m_tablets) {
        result.append(sourceName(entry.id));
    }
    return result;
}

QVariantMap TabletModel::sourceData() const
{
    QVariantMap result;
    result.insert(QStringLiteral("wacomtablet"), QVariantMap{{QStringLiteral("serviceAvailable"), m_serviceAvailable}});
    for (int row = 0; row < m_tablets.size(); ++row) {
        QVariantMap tablet = get(row);
        tablet.remove(QStringLiteral("DataEngineSource"));
        result.insert(sourceName(m_tablets.at(row).id), tablet);
    }
    return result;
}

QVariantMap TabletModel::get(int row) const
{
    if (row < 0 || row >= m_tablets.size()) {
        qCWarning(LOG_WACOM_APPLET) << "get() called with invalid row:" << row;
        return {};
    }
    const auto& entry = m_tablets.at(row);
    QVariantMap map;
    map[QStringLiteral("id")] = entry.id;
    map[QStringLiteral("name")] = entry.name;
    map[QStringLiteral("profiles")] = entry.profiles;
    map[QStringLiteral("currentProfile")] = entry.currentProfile;
    map[QStringLiteral("stylusMode")] = entry.stylusMode;
    map[QStringLiteral("hasTouch")] = entry.hasTouch;
    map[QStringLiteral("touch")] = entry.touch;
    map[QStringLiteral("DataEngineSource")] = sourceName(entry.id);
    return map;
}

void TabletModel::setProfile(const QString& tabletId, const QString& profile)
{
    if (!m_interface) {
        Q_EMIT operationCompleted(QStringLiteral("setProfile"), tabletId, false, QStringLiteral("Service not available"));
        return;
    }
    watchOperations(QStringLiteral("setProfile"), tabletId, {m_interface->setProfile(tabletId, profile)});
}

void TabletModel::setStylusMode(const QString& tabletId, const QString& mode)
{
    if (!m_interface) {
        Q_EMIT operationCompleted(QStringLiteral("setStylusMode"), tabletId, false, QStringLiteral("Service not available"));
        return;
    }
    // Set mode on both stylus and eraser.
    watchOperations(QStringLiteral("setStylusMode"),
        tabletId,
        {m_interface->setProperty(tabletId, s_stylus, s_mode, mode),
            m_interface->setProperty(tabletId, s_eraser, s_mode, mode)},
        [this, tabletId, mode] {
            const int row = findRow(tabletId);
            if (row < 0) {
                return;
            }
            m_tablets[row].stylusMode = mode.compare(s_absolute, Qt::CaseInsensitive) == 0;
            const QModelIndex idx = index(row);
            Q_EMIT dataChanged(idx, idx, {StylusModeRole});
            bumpRevision();
            publishTablet(row);
        });
}

void TabletModel::setRotation(const QString& tabletId, const QString& rotation)
{
    if (!m_interface) {
        Q_EMIT operationCompleted(QStringLiteral("setRotation"), tabletId, false, QStringLiteral("Service not available"));
        return;
    }
    watchOperations(QStringLiteral("setRotation"),
        tabletId,
        {m_interface->setProperty(tabletId, s_stylus, s_rotate, rotation),
            m_interface->setProperty(tabletId, s_eraser, s_rotate, rotation),
            m_interface->setProperty(tabletId, s_touch, s_rotate, rotation)});
}

void TabletModel::setTouch(const QString& tabletId, bool on)
{
    if (!m_interface) {
        Q_EMIT operationCompleted(QStringLiteral("setTouch"), tabletId, false, QStringLiteral("Service not available"));
        return;
    }
    watchOperations(QStringLiteral("setTouch"),
        tabletId,
        {m_interface->setProperty(tabletId, s_touch, s_touchProp, on ? s_on : QStringLiteral("off"))},
        [this, tabletId, on] {
            const int row = findRow(tabletId);
            if (row < 0) {
                return;
            }
            m_tablets[row].touch = on;
            const QModelIndex idx = index(row);
            Q_EMIT dataChanged(idx, idx, {TouchRole});
            bumpRevision();
            publishTablet(row);
        });
}

void TabletModel::watchOperations(const QString& operation,
    const QString& tabletId,
    const QList<QDBusPendingCall>& calls,
    std::function<void()> onSuccess)
{
    struct Result {
        int remaining = 0;
        QString error;
    };
    const auto result = std::make_shared<Result>();
    result->remaining = calls.size();
    const int generation = m_generation;
    const quint64 epoch = m_tabletEpochs.value(tabletId);

    for (const QDBusPendingCall& call : calls) {
        auto* watcher = new QDBusPendingCallWatcher(call, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, operation, tabletId, onSuccess, result, generation, epoch](QDBusPendingCallWatcher* self) {
            const QDBusPendingReply<> reply = *self;
            self->deleteLater();
            if (reply.isError() && result->error.isEmpty()) {
                result->error = reply.error().message();
            }
            if (--result->remaining != 0) {
                return;
            }
            if (generation != m_generation || m_tabletEpochs.value(tabletId) != epoch) {
                Q_EMIT operationCompleted(operation, tabletId, false, QStringLiteral("Tablet or service changed before operation completed"));
                return;
            }
            if (!result->error.isEmpty()) {
                qCWarning(LOG_WACOM_APPLET).noquote() << operation << "failed for" << tabletId << ":" << result->error;
                Q_EMIT operationCompleted(operation, tabletId, false, result->error);
                return;
            }
            if (onSuccess) {
                onSuccess();
            }
            Q_EMIT operationCompleted(operation, tabletId, true, QString());
        });
    }
}

void TabletModel::onServiceRegistered()
{
    m_generation++;
    m_tabletEpochs.clear();
    clearTablets();
    setServiceAvailable(true);
    bumpRevision();

    delete m_interface;
    m_interface = new OrgKdeWacomInterface(s_wacomService, s_wacomPath, QDBusConnection::sessionBus(), this);

    connect(m_interface, &OrgKdeWacomInterface::tabletAdded, this, &TabletModel::onTabletAdded);
    connect(m_interface, &OrgKdeWacomInterface::tabletRemoved, this, &TabletModel::onTabletRemoved);
    connect(m_interface, &OrgKdeWacomInterface::profileChanged, this, &TabletModel::onProfileChanged);

    loadTabletList();
}

void TabletModel::onServiceUnregistered()
{
    m_generation++;
    m_tabletEpochs.clear();
    delete m_interface;
    m_interface = nullptr;
    clearTablets();
    setServiceAvailable(false);
    bumpRevision();
}

void TabletModel::loadTabletList()
{
    if (!m_interface) {
        return;
    }

    QDBusPendingCallWatcher* w = new QDBusPendingCallWatcher(m_interface->getTabletList(), this);
    int gen = m_generation;
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, gen](QDBusPendingCallWatcher* watcher) {
        watcher->deleteLater();
        if (gen != m_generation) {
            return;
        }
        QDBusPendingReply<QStringList> reply(*watcher);
        if (reply.isError()) {
            qCWarning(LOG_WACOM_APPLET).nospace().noquote()
                << "getTabletList failed:" << reply.error().name() << ":" << reply.error().message();
            return;
        }
        const QStringList tablets = reply.value();
        for (const QString& tabletId : tablets) {
            onTabletAdded(tabletId);
        }
    });
}

void TabletModel::onTabletAdded(const QString& tabletId)
{
    if (!m_interface || m_tabletEpochs.contains(tabletId)) {
        return;
    }
    const quint64 epoch = ++m_nextTabletEpoch;
    m_tabletEpochs.insert(tabletId, epoch);
    // Check if it's a touch sensor only device
    QDBusPendingCallWatcher* w = new QDBusPendingCallWatcher(m_interface->isTouchSensor(tabletId), this);
    int gen = m_generation;
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, tabletId, gen, epoch](QDBusPendingCallWatcher* watcher) {
        watcher->deleteLater();
        if (gen != m_generation || m_tabletEpochs.value(tabletId) != epoch) {
            return;
        }
        QDBusPendingReply<bool> reply(*watcher);
        if (reply.isError()) {
            m_tabletEpochs.remove(tabletId);
            qCWarning(LOG_WACOM_APPLET).nospace().noquote()
                << "isTouchSensor failed for" << tabletId << ":" << reply.error().message();
            return;
        }
        if (reply.value()) {
            return; // skip touch-sensor-only devices
        }
        fetchTabletData(tabletId, epoch);
    });
}

void TabletModel::onTabletRemoved(const QString& tabletId)
{
    m_tabletEpochs.remove(tabletId);
    removeTablet(tabletId);
}

void TabletModel::onProfileChanged(const QString& tabletId, const QString& profile)
{
    int row = findRow(tabletId);
    if (row < 0) {
        return;
    }
    // Preserve the DataEngine's -1 sentinel for a profile not in this list.
    int profileIndex = m_tablets[row].profiles.indexOf(profile);
    m_tablets[row].currentProfile = profileIndex;
    QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {CurrentProfileRole});
    bumpRevision();
    publishTablet(row);
}

int TabletModel::findRow(const QString& tabletId) const
{
    for (int i = 0; i < m_tablets.size(); ++i) {
        if (m_tablets[i].id == tabletId) {
            return i;
        }
    }
    return -1;
}

void TabletModel::setServiceAvailable(bool available)
{
    if (m_serviceAvailable == available) {
        return;
    }
    m_serviceAvailable = available;
    Q_EMIT serviceAvailableChanged();
    Q_EMIT sourceDataChanged();
    Q_EMIT newData(QStringLiteral("wacomtablet"), QVariantMap{{QStringLiteral("serviceAvailable"), available}});
}

void TabletModel::bumpRevision()
{
    m_revision++;
    Q_EMIT revisionChanged();
}

void TabletModel::fetchTabletData(const QString& tabletId, quint64 epoch)
{
    if (findRow(tabletId) >= 0) {
        return; // already exists
    }

    const int generation = m_generation;
    QList<QDBusPendingCall> calls;
    calls << m_interface->getInformation(tabletId, s_tabletName)
          << m_interface->listProfiles(tabletId)
          << m_interface->getProfile(tabletId)
          << m_interface->getProperty(tabletId, s_stylus, s_mode)
          << m_interface->getDeviceName(tabletId, s_touch)
          << m_interface->getProperty(tabletId, s_touch, s_touchProp);

    auto* watcher = new MultiDBusPendingCallWatcher(calls, this);
    connect(watcher, &MultiDBusPendingCallWatcher::finished, this, [this, watcher, tabletId, generation, epoch](const QList<QDBusPendingCallWatcher*>& replies) {
        watcher->deleteLater();
        if (generation != m_generation || m_tabletEpochs.value(tabletId) != epoch) {
            return;
        }
        for (QDBusPendingCallWatcher* reply : replies) {
            if (reply->isError()) {
                m_tabletEpochs.remove(tabletId);
                qCWarning(LOG_WACOM_APPLET) << "Tablet discovery failed for" << tabletId << ":" << reply->error().message();
                return;
            }
        }

        const QDBusPendingReply<QString> nameReply = *replies[0];
        const QDBusPendingReply<QStringList> profilesReply = *replies[1];
        const QDBusPendingReply<QString> profileReply = *replies[2];
        const QDBusPendingReply<QString> stylusModeReply = *replies[3];
        const QDBusPendingReply<QString> touchNameReply = *replies[4];
        const QDBusPendingReply<QString> touchModeReply = *replies[5];

        TabletEntry entry;
        entry.id = tabletId;
        entry.name = nameReply.value();
        entry.profiles = profilesReply.value();
        entry.currentProfile = entry.profiles.indexOf(profileReply.value());
        entry.stylusMode = stylusModeReply.value().contains(s_absolute, Qt::CaseInsensitive);
        entry.hasTouch = !touchNameReply.value().isEmpty();
        entry.touch = entry.hasTouch && touchModeReply.value().contains(s_on);
        insertTablet(entry);
    });
}

void TabletModel::insertTablet(const TabletEntry& entry)
{
    if (findRow(entry.id) >= 0) {
        return; // duplicate
    }
    int row = m_tablets.size();
    beginInsertRows(QModelIndex(), row, row);
    m_tablets.append(entry);
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT sourcesChanged();
    Q_EMIT sourceAdded(sourceName(entry.id));
    bumpRevision();
    publishTablet(row);
}

void TabletModel::removeTablet(const QString& tabletId)
{
    int row = findRow(tabletId);
    if (row < 0) {
        return;
    }
    const QString removedSource = sourceName(m_tablets.at(row).id);
    beginRemoveRows(QModelIndex(), row, row);
    m_tablets.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
    Q_EMIT sourcesChanged();
    Q_EMIT sourceRemoved(removedSource);
    Q_EMIT sourceDataChanged();
    bumpRevision();
}

void TabletModel::clearTablets()
{
    if (m_tablets.isEmpty()) {
        return;
    }
    const QStringList oldSources = sources().mid(1);
    beginResetModel();
    m_tablets.clear();
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT sourcesChanged();
    for (const QString& source : oldSources) {
        Q_EMIT sourceRemoved(source);
    }
    Q_EMIT sourceDataChanged();
    bumpRevision();
}

void TabletModel::publishTablet(int row)
{
    if (row < 0 || row >= m_tablets.size()) {
        return;
    }
    const QString source = sourceName(m_tablets.at(row).id);
    QVariantMap values = get(row);
    values.remove(QStringLiteral("DataEngineSource"));
    Q_EMIT sourceDataChanged();
    Q_EMIT newData(source, values);
}

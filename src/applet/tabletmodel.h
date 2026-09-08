/*
 *  SPDX-FileCopyrightText: 2025 SonicDE
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef TABLETMODEL_H
#define TABLETMODEL_H

#include <QAbstractListModel>
#include <QDBusPendingCall>
#include <QHash>
#include <QVariantMap>
#include <QVector>
#include <qqmlintegration.h>

#include <functional>

class OrgKdeWacomInterface;
class QDBusServiceWatcher;

struct TabletEntry {
    QString id;
    QString name;
    QStringList profiles;
    int currentProfile = -1;
    bool stylusMode = true;
    bool hasTouch = false;
    bool touch = false;
};

class TabletModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool serviceAvailable READ serviceAvailable NOTIFY serviceAvailableChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QStringList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantMap sourceData READ sourceData NOTIFY sourceDataChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ProfilesRole,
        CurrentProfileRole,
        StylusModeRole,
        HasTouchRole,
        TouchRole,
        DataEngineSourceRole,
    };
    Q_ENUM(Roles)

    explicit TabletModel(QObject* parent = nullptr);
    ~TabletModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool serviceAvailable() const;
    qulonglong revision() const;
    QStringList sources() const;
    QVariantMap sourceData() const;

    Q_INVOKABLE QVariantMap get(int row) const;

    Q_INVOKABLE void setProfile(const QString& tabletId, const QString& profile);
    Q_INVOKABLE void setStylusMode(const QString& tabletId, const QString& mode);
    Q_INVOKABLE void setRotation(const QString& tabletId, const QString& rotation);
    Q_INVOKABLE void setTouch(const QString& tabletId, bool on);

Q_SIGNALS:
    void countChanged();
    void serviceAvailableChanged();
    void revisionChanged();
    void sourcesChanged();
    void sourceDataChanged();
    void sourceAdded(const QString& sourceName);
    void sourceRemoved(const QString& sourceName);
    void newData(const QString& sourceName, const QVariantMap& data);
    void operationCompleted(const QString& operation, const QString& tabletId, bool success, const QString& error);

private:
    void onServiceRegistered();
    void onServiceUnregistered();
    void loadTabletList();
    void onTabletAdded(const QString& tabletId);
    void onTabletRemoved(const QString& tabletId);
    void onProfileChanged(const QString& tabletId, const QString& profile);

    int findRow(const QString& tabletId) const;
    void setServiceAvailable(bool available);
    void bumpRevision();

    void fetchTabletData(const QString& tabletId, quint64 epoch);
    void insertTablet(const TabletEntry& entry);
    void removeTablet(const QString& tabletId);
    void clearTablets();
    void publishTablet(int row);
    void watchOperations(const QString& operation,
        const QString& tabletId,
        const QList<QDBusPendingCall>& calls,
        std::function<void()> onSuccess = {});

    QVector<TabletEntry> m_tablets;
    bool m_serviceAvailable = false;
    qulonglong m_revision = 0;
    int m_generation = 0;
    // Device lifetimes are independent of service lifetimes: a tablet can be
    // unplugged and reappear with the same ID while old DBus replies arrive.
    quint64 m_nextTabletEpoch = 0;
    QHash<QString, quint64> m_tabletEpochs;

    OrgKdeWacomInterface* m_interface = nullptr;
    QDBusServiceWatcher* m_watcher = nullptr;
};

#endif // TABLETMODEL_H

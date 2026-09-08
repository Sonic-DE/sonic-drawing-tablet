// SPDX-License-Identifier: GPL-2.0-or-later

#include "tabletmodel.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>
#include <utility>

class WacomMock : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Wacom")

public:
    QString lastDevice;
    QString lastProperty;
    QString lastValue;
    QString lastProfile;
    QString requestedDevice;
    int setPropertyCalls = 0;
    QStringList propertyCalls;
    int informationCalls = 0;
    int listProfilesCalls = 0;
    int getProfileCalls = 0;
    int getPropertyCalls = 0;
    int getDeviceNameCalls = 0;
    bool delayInformation = false;
    QList<QDBusMessage> pendingInformation;
    bool delayWrites = false;
    QList<QDBusMessage> pendingWrites;

    void releaseWrites()
    {
        const auto pending = std::exchange(pendingWrites, {});
        for (const auto& request : pending) {
            QDBusConnection(QStringLiteral("wacom-test-service")).send(request.createReply());
        }
    }

    void releaseInformation()
    {
        const auto pending = std::exchange(pendingInformation, {});
        for (const auto& request : pending) {
            QDBusConnection(QStringLiteral("wacom-test-service")).send(request.createReply(QStringLiteral("Test Tablet")));
        }
    }

public Q_SLOTS:
    QStringList getTabletList() const
    {
        return {QStringLiteral("tablet-1")};
    }
    QString getDeviceName(const QString& tabletId, const QString& device)
    {
        ++getDeviceNameCalls;
        requestedDevice = device;
        if (tabletId == QLatin1String("tablet-no-touch")) {
            return {};
        }
        return QStringLiteral("Test Tablet");
    }
    QStringList getDeviceList(const QString&) const
    {
        // The real service returns human-readable XInput device names here,
        // not DeviceType keys. TabletModel must use getDeviceName("touch").
        return {QStringLiteral("Wacom Pen stylus"), QStringLiteral("Wacom Pen eraser"), QStringLiteral("Wacom Finger touch")};
    }
    QString getInformation(const QString&, const QString& key)
    {
        ++informationCalls;
        if (delayInformation) {
            setDelayedReply(true);
            pendingInformation.append(message());
            return {};
        }
        if (key == QLatin1String("TabletName")) {
            return QStringLiteral("Test Tablet");
        }
        if (key == QLatin1String("TouchSensorId")) {
            return QStringLiteral("touch");
        }
        if (key == QLatin1String("Touch")) {
            return QStringLiteral("on");
        }
        if (key == QLatin1String("Mode")) {
            return QStringLiteral("absolute");
        }
        return {};
    }
    QString getProfile(const QString&)
    {
        ++getProfileCalls;
        return QStringLiteral("Default");
    }
    QStringList listProfiles(const QString&)
    {
        ++listProfilesCalls;
        return {QStringLiteral("Default"), QStringLiteral("Drawing")};
    }
    bool isTouchSensor(const QString& tabletId) const
    {
        return tabletId == QLatin1String("touch-sensor-only");
    }
    QString getProperty(const QString&, const QString&, const QString& property)
    {
        ++getPropertyCalls;
        if (property == QLatin1String("Mode")) {
            return QStringLiteral("absolute");
        }
        if (property == QLatin1String("Touch")) {
            return QStringLiteral("on");
        }
        return {};
    }
    void setProfile(const QString&, const QString& profile)
    {
        lastProfile = profile;
    }
    void setProperty(const QString&, const QString& deviceType, const QString& property, const QString& value)
    {
        if (delayWrites) {
            setDelayedReply(true);
            pendingWrites.append(message());
        }
        ++setPropertyCalls;
        propertyCalls.append(QStringLiteral("%1|%2|%3").arg(deviceType, property, value));
        lastDevice = deviceType;
        lastProperty = property;
        lastValue = value;
    }

Q_SIGNALS:
    void tabletAdded(const QString& tabletId);
    void tabletRemoved(const QString& tabletId);
    void profileChanged(const QString& tabletId, const QString& profile);
};

class TabletModelTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("wacom-test-service"));
        QVERIFY(bus.registerService(QStringLiteral("org.kde.Wacom")));
        QVERIFY(bus.registerObject(QStringLiteral("/Tablet"),
            &m_mock,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    }

    void loadsAndUpdatesTabletData()
    {
        TabletModel model;
        QSignalSpy sourceAdded(&model, &TabletModel::sourceAdded);
        QSignalSpy sourceRemoved(&model, &TabletModel::sourceRemoved);
        QSignalSpy newData(&model, &TabletModel::newData);
        QTRY_COMPARE_WITH_TIMEOUT(model.count(), 1, 5000);
        QVERIFY(model.serviceAvailable());
        QCOMPARE(sourceAdded.count(), 1);
        QCOMPARE(sourceAdded.constFirst().constFirst().toString(), QStringLiteral("Tablettablet-1"));
        QVERIFY(newData.count() >= 2); // root availability plus the tablet source

        const QModelIndex first = model.index(0);
        QCOMPARE(model.data(first, TabletModel::IdRole).toString(), QStringLiteral("tablet-1"));
        QCOMPARE(model.data(first, TabletModel::NameRole).toString(), QStringLiteral("Test Tablet"));
        QCOMPARE(model.data(first, TabletModel::DataEngineSourceRole).toString(), QStringLiteral("Tablettablet-1"));
        QCOMPARE(model.get(0).value(QStringLiteral("DataEngineSource")).toString(), QStringLiteral("Tablettablet-1"));
        QCOMPARE(model.sources(), QStringList({QStringLiteral("wacomtablet"), QStringLiteral("Tablettablet-1")}));
        QCOMPARE(model.sourceData().value(QStringLiteral("wacomtablet")).toMap().value(QStringLiteral("serviceAvailable")).toBool(), true);
        QCOMPARE(model.sourceData().value(QStringLiteral("Tablettablet-1")).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Test Tablet"));
        const QVariantMap expectedSourceData{
            {QStringLiteral("id"), QStringLiteral("tablet-1")},
            {QStringLiteral("name"), QStringLiteral("Test Tablet")},
            {QStringLiteral("profiles"), QStringList({QStringLiteral("Default"), QStringLiteral("Drawing")})},
            {QStringLiteral("currentProfile"), 0},
            {QStringLiteral("stylusMode"), true},
            {QStringLiteral("hasTouch"), true},
            {QStringLiteral("touch"), true},
        };
        QCOMPARE(model.sourceData().value(QStringLiteral("Tablettablet-1")).toMap(), expectedSourceData);
        QCOMPARE(m_mock.informationCalls, 1);
        QCOMPARE(m_mock.listProfilesCalls, 1);
        QCOMPARE(m_mock.getProfileCalls, 1);
        QCOMPARE(m_mock.getPropertyCalls, 2);
        QCOMPARE(m_mock.getDeviceNameCalls, 1);
        QCOMPARE(m_mock.requestedDevice, QStringLiteral("touch"));
        QCOMPARE(model.data(first, TabletModel::CurrentProfileRole).toInt(), 0);
        QVERIFY(model.data(first, TabletModel::HasTouchRole).toBool());
        QVERIFY(model.data(first, TabletModel::TouchRole).toBool());
        QVERIFY(model.data(first, TabletModel::StylusModeRole).toBool());

        QSignalSpy completed(&model, &TabletModel::operationCompleted);
        model.setTouch(QStringLiteral("tablet-1"), false);
        QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 5000);
        QCOMPARE(completed.constLast().at(0).toString(), QStringLiteral("setTouch"));
        QCOMPARE(completed.constLast().at(2).toBool(), true);
        QCOMPARE(m_mock.lastDevice, QStringLiteral("touch"));
        QCOMPARE(m_mock.lastProperty, QStringLiteral("Touch"));
        QCOMPARE(m_mock.lastValue, QStringLiteral("off"));
        QCOMPARE(model.data(first, TabletModel::TouchRole).toBool(), false);

        const int callsBeforeModeChange = m_mock.setPropertyCalls;
        m_mock.propertyCalls.clear();
        model.setStylusMode(QStringLiteral("tablet-1"), QStringLiteral("relative"));
        QTRY_COMPARE_WITH_TIMEOUT(m_mock.setPropertyCalls, callsBeforeModeChange + 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(model.data(first, TabletModel::StylusModeRole).toBool(), false, 5000);
        QCOMPARE(m_mock.propertyCalls,
            QStringList({QStringLiteral("stylus|Mode|relative"), QStringLiteral("eraser|Mode|relative")}));

        Q_EMIT m_mock.profileChanged(QStringLiteral("tablet-1"), QStringLiteral("Drawing"));
        QTRY_COMPARE_WITH_TIMEOUT(model.data(first, TabletModel::CurrentProfileRole).toInt(), 1, 5000);

        model.setProfile(QStringLiteral("tablet-1"), QStringLiteral("Default"));
        QTRY_COMPARE_WITH_TIMEOUT(m_mock.lastProfile, QStringLiteral("Default"), 5000);

        const int callsBeforeRotation = m_mock.setPropertyCalls;
        m_mock.propertyCalls.clear();
        model.setRotation(QStringLiteral("tablet-1"), QStringLiteral("cw"));
        QTRY_COMPARE_WITH_TIMEOUT(m_mock.setPropertyCalls, callsBeforeRotation + 3, 5000);
        QCOMPARE(m_mock.lastProperty, QStringLiteral("Rotate"));
        QCOMPARE(m_mock.lastValue, QStringLiteral("cw"));
        QCOMPARE(m_mock.propertyCalls,
            QStringList({QStringLiteral("stylus|Rotate|cw"), QStringLiteral("eraser|Rotate|cw"), QStringLiteral("touch|Rotate|cw")}));

        Q_EMIT m_mock.tabletAdded(QStringLiteral("tablet-no-touch"));
        QTRY_COMPARE_WITH_TIMEOUT(model.count(), 2, 5000);
        const QModelIndex noTouch = model.index(1);
        QCOMPARE(model.data(noTouch, TabletModel::HasTouchRole).toBool(), false);
        QCOMPARE(model.data(noTouch, TabletModel::TouchRole).toBool(), false);

        Q_EMIT m_mock.tabletAdded(QStringLiteral("touch-sensor-only"));
        QTest::qWait(100);
        QCOMPARE(model.count(), 2);

        Q_EMIT m_mock.tabletRemoved(QStringLiteral("tablet-no-touch"));
        QTRY_COMPARE_WITH_TIMEOUT(model.count(), 1, 5000);
        QCOMPARE(sourceRemoved.constLast().constFirst().toString(), QStringLiteral("Tablettablet-no-touch"));

        QDBusConnection bus(QStringLiteral("wacom-test-service"));
        QVERIFY(bus.unregisterService(QStringLiteral("org.kde.Wacom")));
        QTRY_VERIFY_WITH_TIMEOUT(!model.serviceAvailable(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(model.count(), 0, 5000);
        QCOMPARE(model.sources(), QStringList({QStringLiteral("wacomtablet")}));
        QCOMPARE(model.sourceData().value(QStringLiteral("wacomtablet")).toMap().value(QStringLiteral("serviceAvailable")).toBool(), false);

        QVERIFY(bus.registerService(QStringLiteral("org.kde.Wacom")));
        QTRY_VERIFY_WITH_TIMEOUT(model.serviceAvailable(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(model.count(), 1, 5000);
    }

    void removedTabletCannotBeResurrectedByDiscovery()
    {
        TabletModel model;
        QTRY_COMPARE(model.count(), 1);
        QSignalSpy removed(&model, &TabletModel::sourceRemoved);
        m_mock.delayInformation = true;
        const int profilesBefore = m_mock.getProfileCalls;
        Q_EMIT m_mock.tabletAdded(QStringLiteral("delayed"));
        QTRY_COMPARE(m_mock.pendingInformation.size(), 1);
        // The other five discovery calls must run before the name reply.
        QTRY_COMPARE(m_mock.getProfileCalls, profilesBefore + 1);
        Q_EMIT m_mock.tabletRemoved(QStringLiteral("delayed"));
        // A subsequent signal is a barrier: both are delivered in order.
        Q_EMIT m_mock.tabletRemoved(QStringLiteral("tablet-1"));
        QTRY_COMPARE(removed.count(), 1);
        m_mock.releaseInformation();
        m_mock.delayInformation = false;
        QTest::qWait(100);
        QCOMPARE(model.count(), 0);
        QVERIFY(!model.sources().contains(QStringLiteral("Tabletdelayed")));
    }

    void unknownProfileClearsOldSelection()
    {
        TabletModel model;
        QTRY_COMPARE(model.count(), 1);
        QSignalSpy updates(&model, &TabletModel::newData);
        Q_EMIT m_mock.profileChanged(QStringLiteral("tablet-1"), QStringLiteral("New external profile"));
        QTRY_VERIFY(!updates.isEmpty());
        QCOMPARE(model.get(0).value(QStringLiteral("currentProfile")).toInt(), -1);
    }

    void oldWriteReplyCannotChangeReconnectedTablet()
    {
        TabletModel model;
        QTRY_COMPARE(model.count(), 1);
        QSignalSpy completed(&model, &TabletModel::operationCompleted);
        m_mock.delayWrites = true;
        model.setTouch(QStringLiteral("tablet-1"), false);
        QTRY_COMPARE(m_mock.pendingWrites.size(), 1);
        Q_EMIT m_mock.tabletRemoved(QStringLiteral("tablet-1"));
        QTRY_COMPARE(model.count(), 0);
        Q_EMIT m_mock.tabletAdded(QStringLiteral("tablet-1"));
        QTRY_COMPARE(model.count(), 1);
        QVERIFY(model.get(0).value(QStringLiteral("touch")).toBool());
        m_mock.releaseWrites();
        m_mock.delayWrites = false;
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(completed.constFirst().at(2).toBool(), false);
        QVERIFY(model.get(0).value(QStringLiteral("touch")).toBool());
    }

private:
    WacomMock m_mock;
};

QTEST_GUILESS_MAIN(TabletModelTest)

#include "tabletmodeltest.moc"

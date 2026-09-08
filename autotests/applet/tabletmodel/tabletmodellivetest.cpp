// SPDX-License-Identifier: GPL-2.0-or-later

#include "tabletmodel.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QSignalSpy>
#include <QTest>

class TabletModelLiveTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void matchesPhysicalTabletService()
    {
        QDBusInterface service(QStringLiteral("org.kde.Wacom"),
            QStringLiteral("/Tablet"),
            QStringLiteral("org.kde.Wacom"),
            QDBusConnection::sessionBus());
        QVERIFY2(service.isValid(), "org.kde.Wacom is not available on the session bus");

        const QDBusReply<QStringList> tabletList = service.call(QStringLiteral("getTabletList"));
        QVERIFY(tabletList.isValid());
        QVERIFY2(!tabletList.value().isEmpty(), "No physical tablet was reported by org.kde.Wacom");

        QStringList expectedTablets;
        for (const QString& tabletId : tabletList.value()) {
            const QDBusReply<bool> touchSensor = service.call(QStringLiteral("isTouchSensor"), tabletId);
            QVERIFY(touchSensor.isValid());
            if (!touchSensor.value()) {
                expectedTablets.append(tabletId);
            }
        }
        QVERIFY2(!expectedTablets.isEmpty(), "Only touch-sensor companion devices were reported");

        TabletModel model;
        QTRY_COMPARE_WITH_TIMEOUT(model.count(), expectedTablets.size(), 10000);
        QVERIFY(model.serviceAvailable());

        for (const QString& tabletId : expectedTablets) {
            int row = -1;
            for (int candidate = 0; candidate < model.count(); ++candidate) {
                if (model.get(candidate).value(QStringLiteral("id")).toString() == tabletId) {
                    row = candidate;
                    break;
                }
            }
            QVERIFY2(row >= 0, qPrintable(QStringLiteral("Tablet %1 is absent from TabletModel").arg(tabletId)));

            const QVariantMap data = model.get(row);
            QCOMPARE(data.value(QStringLiteral("DataEngineSource")).toString(), QStringLiteral("Tablet%1").arg(tabletId));
            QVERIFY(model.sources().contains(QStringLiteral("Tablet%1").arg(tabletId)));
            const QVariantMap namedSource = model.sourceData().value(QStringLiteral("Tablet%1").arg(tabletId)).toMap();

            const QDBusReply<QString> name = service.call(QStringLiteral("getInformation"), tabletId, QStringLiteral("TabletName"));
            const QDBusReply<QStringList> profiles = service.call(QStringLiteral("listProfiles"), tabletId);
            const QDBusReply<QString> profile = service.call(QStringLiteral("getProfile"), tabletId);
            const QDBusReply<QString> stylusMode = service.call(QStringLiteral("getProperty"), tabletId, QStringLiteral("stylus"), QStringLiteral("Mode"));
            const QDBusReply<QString> touchName = service.call(QStringLiteral("getDeviceName"), tabletId, QStringLiteral("touch"));
            const QDBusReply<QString> touchMode = service.call(QStringLiteral("getProperty"), tabletId, QStringLiteral("touch"), QStringLiteral("Touch"));

            QVERIFY(name.isValid());
            QVERIFY(profiles.isValid());
            QVERIFY(profile.isValid());
            QVERIFY(stylusMode.isValid());
            QVERIFY(touchName.isValid());
            QVERIFY(touchMode.isValid());

            QCOMPARE(data.value(QStringLiteral("name")).toString(), name.value());
            QCOMPARE(namedSource.value(QStringLiteral("name")).toString(), name.value());
            QCOMPARE(data.value(QStringLiteral("profiles")).toStringList(), profiles.value());
            QCOMPARE(data.value(QStringLiteral("currentProfile")).toInt(), profiles.value().indexOf(profile.value()));
            QCOMPARE(data.value(QStringLiteral("stylusMode")).toBool(), stylusMode.value().contains(QStringLiteral("absolute"), Qt::CaseInsensitive));
            QCOMPARE(data.value(QStringLiteral("hasTouch")).toBool(), !touchName.value().isEmpty());
            QCOMPARE(data.value(QStringLiteral("touch")).toBool(), !touchName.value().isEmpty() && touchMode.value().contains(QStringLiteral("on")));

            // Exercise state-changing paths with their existing values, then
            // verify the daemon still reports those values. This avoids
            // changing the user's tablet configuration.
            QSignalSpy completed(&model, &TabletModel::operationCompleted);
            model.setProfile(tabletId, profile.value());
            QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 5000);
            QCOMPARE(completed.takeLast().at(2).toBool(), true);
            QCOMPARE(QDBusReply<QString>(service.call(QStringLiteral("getProfile"), tabletId)).value(), profile.value());

            const QString requestedMode = stylusMode.value().toLower();
            model.setStylusMode(tabletId, requestedMode);
            QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 5000);
            QCOMPARE(completed.takeLast().at(2).toBool(), true);
            const QDBusReply<QString> resultingMode = service.call(QStringLiteral("getProperty"), tabletId, QStringLiteral("stylus"), QStringLiteral("Mode"));
            QVERIFY(resultingMode.isValid());
            QCOMPARE(resultingMode.value().toLower(), requestedMode);

            const QDBusReply<QString> rotation = service.call(QStringLiteral("getProperty"), tabletId, QStringLiteral("stylus"), QStringLiteral("Rotate"));
            QVERIFY(rotation.isValid());
            if (!rotation.value().isEmpty()) {
                model.setRotation(tabletId, rotation.value());
                QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 5000);
                QCOMPARE(completed.takeLast().at(2).toBool(), true);
                const QDBusReply<QString> resultingRotation = service.call(QStringLiteral("getProperty"), tabletId, QStringLiteral("stylus"), QStringLiteral("Rotate"));
                QVERIFY(resultingRotation.isValid());
                QCOMPARE(resultingRotation.value(), rotation.value());
            }
        }
    }
};

QTEST_GUILESS_MAIN(TabletModelLiveTest)

#include "tabletmodellivetest.moc"

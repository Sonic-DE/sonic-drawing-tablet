/*
 * SPDX-FileCopyrightText: 2015 Weng Xuetian <wengxt@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDBusPendingCallWatcher>
#include <QObject>

class MultiDBusPendingCallWatcher : public QObject {
    Q_OBJECT

public:
    explicit MultiDBusPendingCallWatcher(const QList<QDBusPendingCall>& calls, QObject* parent = nullptr);

Q_SIGNALS:
    void finished(const QList<QDBusPendingCallWatcher*>& watchers);

private:
    void callFinished();

    QList<QDBusPendingCallWatcher*> m_watchers;
    int m_unfinished = 0;
};

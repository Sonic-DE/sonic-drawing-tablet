/*
 * SPDX-FileCopyrightText: 2015 Weng Xuetian <wengxt@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "multidbuspendingcallwatcher.h"

#include <QMetaObject>

MultiDBusPendingCallWatcher::MultiDBusPendingCallWatcher(const QList<QDBusPendingCall>& calls, QObject* parent)
    : QObject(parent)
{
    m_watchers.reserve(calls.size());
    for (const QDBusPendingCall& call : calls) {
        auto* watcher = new QDBusPendingCallWatcher(call, this);
        m_watchers.append(watcher);
        if (!watcher->isFinished()) {
            ++m_unfinished;
            connect(watcher, &QDBusPendingCallWatcher::finished, this, [this] {
                callFinished();
            });
        }
    }

    if (m_unfinished == 0) {
        QMetaObject::invokeMethod(this, [this] { Q_EMIT finished(m_watchers); }, Qt::QueuedConnection);
    }
}

void MultiDBusPendingCallWatcher::callFinished()
{
    if (--m_unfinished == 0) {
        Q_EMIT finished(m_watchers);
    }
}

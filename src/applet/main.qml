// -*- coding: iso-8859-1 -*-
/*
 *   Copyright 2015 Weng Xuetian <wengxt@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kcmutils as KCMUtils
import org.kde.config as KConfig

pragma ComponentBehavior: Bound

PlasmoidItem {
    id: root

    PlasmaCore.Action {
        id: configureAction
        text: i18n("&Configure Graphics Tablet...") // qmllint disable unqualified
        icon.name: "configure"
        visible: KConfig.KAuthorized.authorizeControlModule("kcm_wacomtablet");
        onTriggered: KCMUtils.KCMLauncher.openSystemSettings("kcm_wacomtablet");
    }

    Component.onCompleted: {
        Plasmoid.setInternalAction("configure", configureAction)
    }

    readonly property TabletModel tabletModel: TabletModel {
        id: tabletModelInstance
    }

    property bool active: tabletModelInstance.serviceAvailable && tabletModelInstance.count != 0

    toolTipMainText: i18n("Wacom Tablet") // qmllint disable unqualified
    Plasmoid.status: active ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    Plasmoid.icon: "input-tablet"

    fullRepresentation: FullRepresentation {
        tabletModel: root.tabletModel
        active: root.active
    }
}

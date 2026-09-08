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
import QtQuick.Layouts
import org.kde.plasma.components as PC3
import org.kde.ksvg as KSvg
import org.kde.kirigami as Kirigami

Item {
    id: root

    required property TabletModel tabletModel
    required property bool active

    function defaultValue(value, d) {
        return (typeof value == 'undefined') ? d : value;
    }

    function deviceLabel() {
        if (tabletModel.serviceAvailable) {
            if (tabletModel.count == 0) {
                return i18n("Graphic Tablet - Device not detected."); // qmllint disable unqualified
            } else {
                if (tabletComboBox.currentIndex >= 0) {
                    return currentTablet.name;
                }
                return "";
            }
        }

        return i18n("Error - Tablet service not available."); // qmllint disable unqualified
    }

    readonly property var currentTablet: {
        // read revision first so role-only updates reevaluate this binding
        const _rev = tabletModel.revision;
        if (tabletComboBox.currentIndex >= 0 && tabletComboBox.currentIndex < tabletModel.count) {
            return tabletModel.get(tabletComboBox.currentIndex);
        }
        return ({});
    }

    KSvg.Svg {
        id: lineSvg
        imagePath: "widgets/line"
    }

    Row {
        id: title
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            id: titleIcon
            source: "input-tablet"
            width: Kirigami.Units.iconSizes.medium
            height: width
        }
        PC3.Label {
            id: deviceNameLabel
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - titleIcon.width - parent.spacing
            text: root.deviceLabel()
            wrapMode: Text.Wrap
        }
    }

    KSvg.SvgItem {
        id: separator
        anchors {
            top: title.bottom
            left: parent.left
            right: parent.right
        }
        elementId: "horizontal-line"
        svg: lineSvg
        height: lineSvg.elementSize("horizontal-line").height
    }

    Row {
        anchors {
            top: separator.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            topMargin: Kirigami.Units.smallSpacing
            leftMargin: Kirigami.Units.smallSpacing
        }
        visible: !root.active
        spacing: Kirigami.Units.smallSpacing * 2

        Kirigami.Icon {
            id: errorIcon
            width: Kirigami.Units.iconSizes.medium
            height: width
            source: "dialog-warning"
        }
        PC3.Label {
            id: errorLabel
            width: parent.width - errorIcon.width - parent.spacing
            text: root.tabletModel.serviceAvailable ?
                i18n("This widget is inactive because your tablet device is not connected or currently not supported.") : // qmllint disable unqualified
                i18n("Please start the KDE wacom tablet service.\nThe service is required for tablet detection and profile support.") // qmllint disable unqualified
            wrapMode: Text.Wrap
        }
    }

    function setProfile() {
        profileComboBox.currentIndex = defaultValue(currentTablet.currentProfile, -1);
    }

    GridLayout {
        id: selector
        anchors {
            left: parent.left
            right: parent.right
            top: separator.bottom
            topMargin: Kirigami.Units.smallSpacing
        }
        visible: root.active
        columns: 2
        PC3.Label {
            text: i18n("Select Tablet:") // qmllint disable unqualified
        }

        PC3.ComboBox {
            id: tabletComboBox
            Layout.fillWidth: true
            model: root.tabletModel
            textRole: "name"
            onCurrentIndexChanged: {
                profileModel.clear();
                profileComboBox.currentIndex = -1;
                if (tabletComboBox.currentIndex < 0) {
                    return;
                }

                var profiles = root.currentTablet.profiles;
                if (typeof profiles == 'undefined') {
                    return;
                }
                for (var i = 0; i < profiles.length; i++) {
                    profileModel.append({"name" : profiles[i]});
                }

                root.setProfile();
            }
        }

        PC3.Label {
            text: i18n("Select Profile:") // qmllint disable unqualified
        }

        ListModel {
            id: profileModel
        }

        PC3.ComboBox {
            id: profileComboBox
            Layout.fillWidth: true
            model: profileModel
            textRole: "name"

            onActivated: function(index) {
                if (tabletComboBox.currentIndex < 0) {
                    return;
                }
                root.tabletModel.setProfile(root.currentTablet.id, profileModel.get(index).name);
            }
        }
    }

    Connections {
        target: root.tabletModel
        function onCountChanged() {
            tabletComboBox.currentIndex = -1;
            if (root.tabletModel.count > 0) {
                tabletComboBox.currentIndex = 0;
            }
        }

        function onRevisionChanged() {
            if (tabletComboBox.currentIndex < 0) {
                return;
            }
            root.setProfile();
        }
    }

    PC3.GroupBox {
        visible: root.active
        anchors {
            left: parent.left
            right: parent.right
            top: selector.bottom
            bottom: parent.bottom
            topMargin: Kirigami.Units.smallSpacing
        }
        title: i18nc( "Groupbox Settings for the applet to change some values on the fly", "Settings" ) // qmllint disable unqualified
        GridLayout {
            columns: 2
            PC3.Label {
                visible: root.defaultValue(root.currentTablet.hasTouch, false)
                text: i18nc( "Toggle between touch on/off", "Touch:" ) // qmllint disable unqualified
            }

            PC3.CheckBox {
                visible: root.defaultValue(root.currentTablet.hasTouch, false);
                checked: root.defaultValue(root.currentTablet.touch, false);
                onClicked: {
                    if (tabletComboBox.currentIndex >= 0) {
                        root.tabletModel.setTouch(root.currentTablet.id, checked);
                    }
                }
            }


            PC3.Label {
                text: i18nc( "Rotation of the tablet pad", "Rotation:" ) // qmllint disable unqualified
            }

            RowLayout {
                RotationButton {
                    tabletRotation: "none"
                    icon.name: "input-tablet"
                    tabletModel: root.tabletModel
                    tabletId: root.currentTablet.id ?? ""
                    PC3.ToolTip.text: i18nc("Either no orientation or the current screen orientation is applied to the tablet.", "Default Orientation"); // qmllint disable unqualified
                    PC3.ToolTip.delay: Kirigami.Units.toolTipDelay
                    PC3.ToolTip.visible: hovered
                }
                RotationButton {
                    tabletRotation: "cw"
                    icon.name: "object-rotate-left"
                    tabletModel: root.tabletModel
                    tabletId: root.currentTablet.id ?? ""
                    PC3.ToolTip.text: i18nc("The tablet will be rotated clockwise.", "Rotate Tablet Clockwise") // qmllint disable unqualified
                    PC3.ToolTip.delay: Kirigami.Units.toolTipDelay
                    PC3.ToolTip.visible: hovered
                }
                RotationButton {
                    tabletRotation: "ccw"
                    icon.name: "object-rotate-right"
                    tabletModel: root.tabletModel
                    tabletId: root.currentTablet.id ?? ""
                    PC3.ToolTip.text: i18nc("The tablet will be rotated counterclockwise.", "Rotate Tablet Counterclockwise") // qmllint disable unqualified
                    PC3.ToolTip.delay: Kirigami.Units.toolTipDelay
                    PC3.ToolTip.visible: hovered
                }
                RotationButton {
                    tabletRotation: "half"
                    icon.name: "object-flip-vertical"
                    tabletModel: root.tabletModel
                    tabletId: root.currentTablet.id ?? ""
                    PC3.ToolTip.text: i18nc("The tablet will be rotated up side down.", "Rotate Tablet Upside-Down") // qmllint disable unqualified
                    PC3.ToolTip.delay: Kirigami.Units.toolTipDelay
                    PC3.ToolTip.visible: hovered
                }
            }
            PC3.Label {
                text: i18nc( "Toggle between absolute/relative penmode", "Mode:" ) // qmllint disable unqualified
            }
            RowLayout {
                PC3.RadioButton {
                    text: i18nc( "absolute pen movement (pen mode)", "Absolute" ) // qmllint disable unqualified
                    checked: root.defaultValue(root.currentTablet.stylusMode, true);
                    onClicked : {
                        if (tabletComboBox.currentIndex < 0) {
                            return;
                        }
                        root.tabletModel.setStylusMode(root.currentTablet.id, "absolute");
                    }
                }
                PC3.RadioButton {
                    text: i18nc( "relative pen movement (mouse mode)", "Relative" ) // qmllint disable unqualified
                    checked: !root.defaultValue(root.currentTablet.stylusMode, true)
                    onClicked : {
                        if (tabletComboBox.currentIndex < 0) {
                            return;
                        }
                        root.tabletModel.setStylusMode(root.currentTablet.id, "relative");
                    }
                }
            }
        }
    }
}

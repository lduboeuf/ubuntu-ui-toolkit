/*
 * Copyright 2016 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.4
import Ubuntu.Components 1.3
import Ubuntu.Components.Popups 1.3
import Ubuntu.Components.Styles 1.3 as Style
import QtGraphicalEffects 1.0

Style.ActionBarStyle {
    id: actionBarStyle
    implicitHeight: units.gu(5)
    //        implicitWidth: units.gu(4) * styledItem.numberOfItems
    implicitWidth: units.gu(36) // 9 * defaultDelegate.width

    overflowIconName: "contextual-menu"

    // Unused with the standard action icon buttons, but may be used with a custom delegate.
    overflowText: "More"

    /*!
      The default action delegate if the styled item does
      not provide a delegate.
     */
    // FIXME: This ListItem { AbstractButton { } } construction can be avoided
    //  when StyledItem supports cursor keys navigation, see bug #1573616
    defaultDelegate: ListItem {
        width: units.gu(4)
        height: units.gu(4)
        AbstractButton {
            anchors.fill: parent
            style: IconButtonStyle { }
            objectName: action.objectName + "_button"
            height: parent ? parent.height : undefined
            action: modelData
            //        activeFocusOnTab: false
        }
        divider.visible: false
    }

    defaultNumberOfSlots: 3


    Rectangle {
        id: listViewContainer
        anchors {
            fill: parent
        }
        //        color: "pink"
        color: "transparent"
        //        opacity: 0.5
        clip: true

        property real listViewMargins: units.gu(2)


        property var visibleActions: getReversedVisibleActions(styledItem.actions)
        function getReversedVisibleActions(actions) {
            var visibleActionList = [];
//            for (var i in actions) {
            for (var i=actions.length-1; i >=0; i--) {
                var action = actions[i];
                if (action && action.hasOwnProperty("visible") && action.visible) {
                    visibleActionList.push(action);
                }
            }
            return visibleActionList;
        }
//        function getReversedActions(actions) {
//            var newlist = [];
//            for (var i=actions.length-1; i >= 0; i--) {
//                newlist.push(actions[i]);
//            }
//            return newlist;
//        }

        ListView {
            id: actionsListView
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
                //                leftMargin: listViewContainer.listViewMargins
                //                rightMargin: listViewContainer.listViewMargins
            }
            //            width: Math.min(listViewContainer.width - 2*listViewContainer.listViewMargins,
            //                            contentWidth)
            width: listViewContainer.width

            clip: true
            orientation: ListView.Horizontal
            boundsBehavior: Flickable.StopAtBounds
            delegate: styledItem.delegate
            model: listViewContainer.visibleActions

            onCurrentIndexChanged: print("current index = "+currentIndex)

            SmoothedAnimation {
                objectName: "actions_scroll_animation"
                id: contentXAnim
                target: actionsListView
                property: "contentX"
                duration: UbuntuAnimation.FastDuration
                velocity: units.gu(10)
            }
        }
    }
    MouseArea {
        // Detect hovering over the left and right areas to show the scrolling chevrons.
        id: hoveringArea

        property real iconsDisabledOpacity: 0.3

        anchors.fill: parent
        hoverEnabled: true

        property bool pressedLeft: false
        property bool pressedRight: false
        onPressed: {
            pressedLeft = leftIcon.contains(mouse);
            pressedRight = rightIcon.contains(mouse);
            mouse.accepted = pressedLeft || pressedRight;
        }
        onClicked: {
            // positionViewAtIndex() does not provide animation
            var scrollDirection = 0;
            if (pressedLeft && leftIcon.contains(mouse)) {
                scrollDirection = -1;
            } else if (pressedRight && rightIcon.contains(mouse)) {
                scrollDirection = 1;
            } else {
                // User pressed on the left or right icon, and then released inside of the
                // MouseArea but outside of the icon that was pressed.
                return;
            }
            if (contentXAnim.running) contentXAnim.stop();
            var newContentX = actionsListView.contentX + (actionsListView.width * scrollDirection);
            contentXAnim.from = actionsListView.contentX;
            // make sure we don't overshoot bounds
            contentXAnim.to = MathUtils.clamp(
                        newContentX,
                        0.0, // estimation of originX is some times wrong when scrolling towards the beginning
                        actionsListView.originX + actionsListView.contentWidth - actionsListView.width);
            contentXAnim.start();
        }

        Icon {
            id: leftIcon
            objectName: "left_scroll_icon"
            // return whether the pressed event was done inside the area of the icon
            function contains(mouse) {
                return (mouse.x < listViewContainer.listViewMargins &&
                        !actionsListView.atXBeginning);
            }
            anchors {
                left: parent.left
                leftMargin: (listViewContainer.listViewMargins - width) / 2
                //                    bottom: parent.bottom
                //                    bottomMargin: sectionsStyle.height < units.gu(4) ? units.gu(1) : units.gu(2)
                verticalCenter: parent.verticalCenter
            }
            width: units.gu(1)
            height: units.gu(1)
            visible: false
            rotation: 180
            opacity: visible
                     ? actionsListView.atXBeginning ? hoveringArea.iconsDisabledOpacity : 1.0 : 0.0
            name: "chevron"
            Behavior on opacity {
                UbuntuNumberAnimation {
                    duration: UbuntuAnimation.FastDuration
                }
            }
        }

        Icon {
            id: rightIcon
            objectName: "right_scroll_icon"
            // return whether the pressed event was done inside the area of the icon
            function contains(mouse) {
                return (mouse.x > (hoveringArea.width - listViewContainer.listViewMargins) &&
                        !actionsListView.atXEnd);
            }
            anchors {
                right: parent.right
                rightMargin: (listViewContainer.listViewMargins - width) / 2
                //                    bottom: parent.bottom
                //                    bottomMargin: sectionsStyle.height < units.gu(4) ? units.gu(1) : units.gu(2)
                verticalCenter: parent.verticalCenter
            }
            width: units.gu(1)
            height: units.gu(1)
            visible: false
            opacity: visible
                     ? actionsListView.atXEnd ? hoveringArea.iconsDisabledOpacity : 1.0 : 0.0
            name: "chevron"
            Behavior on opacity {
                UbuntuNumberAnimation {
                    duration: UbuntuAnimation.FastDuration
                }
            }
        }
    }

    LinearGradient {
//    Rectangle {
        id: gradient
//        color: "pink"
        visible: false
        anchors.fill: parent
        start: Qt.point(0,0)
        end: Qt.point(width,0)

        property real gradientWidth: listViewContainer.listViewMargins / gradient.width
        //the width is gradientWidth, but we want the gradient to actually start/finish at gradientSplitPosition
        //just to leave some margin.
        property real gradientSplitPosition: 2/3 * gradientWidth

        gradient: Gradient {
            //left gradient
            GradientStop { position: 0.0 ; color: Qt.rgba(1,1,1,0) }
            GradientStop { position: gradient.gradientSplitPosition ; color: Qt.rgba(1,1,1,0) }
            GradientStop { position: gradient.gradientWidth; color: Qt.rgba(1,1,1,1) }
            //right gradient
            GradientStop { position: 1.0 - gradient.gradientWidth; color: Qt.rgba(1,1,1,1) }
            GradientStop { position: 1.0 - gradient.gradientSplitPosition; color: Qt.rgba(1,1,1,0) }
            GradientStop { position: 1.0; color: Qt.rgba(1,1,1,0) }
        }
    }

    OpacityMask {
        id: mask
        anchors.fill: parent
        visible: false
        source: listViewContainer
        maskSource: gradient
    }

    states: [
        State {
            name: "hovering"
            when: hoveringArea.containsMouse
            PropertyChanges { target: mask; visible: true }
            PropertyChanges { target: listViewContainer; opacity: 0.0 }
            PropertyChanges { target: leftIcon; visible: true }
            PropertyChanges { target: rightIcon; visible: true }
        }
    ]
}

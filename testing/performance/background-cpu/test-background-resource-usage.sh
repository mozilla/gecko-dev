#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
ACTIVITY="org.mozilla.fenix.HomeActivity"
XML_FILE=$TESTING_DIR/window_dump.xml
XMLSTARLET_CMD=${XMLSTARLET:-xmlstarlet}

URL_MOZILLA="https://www.mozilla.org/"

adb shell am start -n "$BROWSER_BINARY/$ACTIVITY"
sleep 4

adb shell uiautomator dump
adb pull /sdcard/window_dump.xml $XML_FILE
sleep 1

# calculate toolbar coordinates
TOOLBAR_BOUNDS=$($XMLSTARLET_CMD sel -t -v '//node[@resource-id = "'$BROWSER_BINARY':id/toolbar"]/@bounds' $XML_FILE)
sleep 1

toolbar_x1=$(($(echo "$TOOLBAR_BOUNDS" | awk -F'[][]' '{print $2}' | awk -F',' '{print $1}')))
toolbar_x2=$(($(echo "$TOOLBAR_BOUNDS" | awk -F'[][]' '{print $4}' | awk -F',' '{print $1}')))
sum_toolbar_x=$(($toolbar_x1+$toolbar_x2))

toolbar_y1=$(($(echo "$TOOLBAR_BOUNDS" | awk -F'[][]' '{print $2}' | awk -F',' '{print $2}')))
toolbar_y2=$(($(echo "$TOOLBAR_BOUNDS" | awk -F'[][]' '{print $4}' | awk -F',' '{print $2}')))
sum_toolbar_y=$(($toolbar_y1+$toolbar_y2))

TOOLBAR_X_COORDINATE=$(($sum_toolbar_x/2))
TOOLBAR_Y_COORDINATE=$(($sum_toolbar_y/2))

# calculate tabs tray coordinates
TABS_TRAY_BUTTON_BOUNDS=$($XMLSTARLET_CMD sel -t -v '//node[@resource-id = "'$BROWSER_BINARY':id/counter_box"]/@bounds' $XML_FILE)
sleep 1

tabs_tray_x1=$(($(echo "$TABS_TRAY_BUTTON_BOUNDS" | awk -F'[][]' '{print $2}' | awk -F',' '{print $1}')))
tabs_tray_x2=$(($(echo "$TABS_TRAY_BUTTON_BOUNDS" | awk -F'[][]' '{print $4}' | awk -F',' '{print $1}')))
sum_tabs_tray_x=$(($tabs_tray_x1+$tabs_tray_x2))

tabs_tray_y1=$(($(echo "$TABS_TRAY_BUTTON_BOUNDS" | awk -F'[][]' '{print $2}' | awk -F',' '{print $2}')))
tabs_tray_y2=$(($(echo "$TABS_TRAY_BUTTON_BOUNDS" | awk -F'[][]' '{print $4}' | awk -F',' '{print $2}')))
sum_tabs_tray_y=$(($tabs_tray_y1+$tabs_tray_y2))

TABS_TRAY_BUTTON_X_COORDINATE=$(($sum_tabs_tray_x/2))
TABS_TRAY_BUTTON_Y_COORDINATE=$(($sum_tabs_tray_y/2))

adb shell input tap $TABS_TRAY_BUTTON_X_COORDINATE $TABS_TRAY_BUTTON_Y_COORDINATE
sleep 2

adb shell uiautomator dump
adb pull /sdcard/window_dump.xml $XML_FILE

# calculate new tab button coordinates
ADD_TAB_BUTTON_BOUNDS=$($XMLSTARLET_CMD sel -t -v '//node[@content-desc="Add tab"]/@bounds' $XML_FILE)
sleep 1

add_tab_x1=$(($(echo "$ADD_TAB_BUTTON_BOUNDS" | awk -F'[][]' '{print $2}' | awk -F',' '{print $1}')))
add_tab_x2=$(($(echo "$ADD_TAB_BUTTON_BOUNDS" | awk -F'[][]' '{print $4}' | awk -F',' '{print $1}')))
sum_add_tab_x=$(($add_tab_x1+$add_tab_x2))

add_tab_y1=$(($(echo "$ADD_TAB_BUTTON_BOUNDS" | awk -F'[][]' '{print $2}' | awk -F',' '{print $2}')))
add_tab_y2=$(($(echo "$ADD_TAB_BUTTON_BOUNDS" | awk -F'[][]' '{print $4}' | awk -F',' '{print $2}')))
sum_add_tab_y=$(($add_tab_y1+$add_tab_y2))

ADD_TAB_BUTTON_X_COORDINATE=$(($sum_add_tab_x/2))
ADD_TAB_BUTTON_Y_COORDINATE=$(($sum_add_tab_y/2))

rm $XML_FILE

# go back to main page to start testing
adb shell input keyevent KEYCODE_BACK
sleep 1

function tapToFocusToolbar() {
    adb shell input tap $TOOLBAR_X_COORDINATE $TOOLBAR_Y_COORDINATE
    sleep 2
}

function inputTextToToolbar() {
    adb shell input text $1
    sleep 2
}

function tapEnterAndWait5s() {
    adb shell input keyevent 66
    sleep 5
}

function tapEnterAndWait10s() {
    adb shell input keyevent 66
    sleep 10
}

function performScrollDown() {
    adb shell input swipe 500 500 500 300
    adb shell input swipe 500 500 500 300
    adb shell input swipe 500 500 500 300
    sleep 2
}

function performScrollUp() {
    adb shell input swipe 500 300 500 500
    adb shell input swipe 500 300 500 500
    adb shell input swipe 500 300 500 500
    sleep 2
}

function tapToOpenTabsTray() {
    adb shell input tap $TABS_TRAY_BUTTON_X_COORDINATE $TABS_TRAY_BUTTON_Y_COORDINATE
    sleep 2
}

function tapToAddTab() {
    adb shell input tap $ADD_TAB_BUTTON_X_COORDINATE $ADD_TAB_BUTTON_Y_COORDINATE
    sleep 3
}

function addTab() {
    tapToOpenTabsTray
    tapToAddTab
}

function surfingSingleSite() {
    tapToFocusToolbar
    inputTextToToolbar $1
    tapEnterAndWait10s
    performScrollDown
    performScrollUp
}

function appToBackground() {
    adb shell input keyevent KEYCODE_HOME
    sleep 2
}

# at this point our system is ready, the buttons' coordinates are generated
# test starts after this line
touch $TESTING_DIR/test_start.signal

surfingSingleSite $URL_MOZILLA
appToBackground
sleep 600 # wait 10 mins in the background

tail -8 $TESTING_DIR/tmp.txt
touch $TESTING_DIR/test_end.signal
adb shell am force-stop $BROWSER_BINARY

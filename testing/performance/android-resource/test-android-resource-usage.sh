#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
ACTIVITY="org.mozilla.fenix.HomeActivity"
TOOLBAR_BOUNDS_ID="toolbar"
TABS_TRAY_BUTTON_BOUNDS_ID="counter_box"

XML_FILE=$TESTING_DIR/window_dump.xml
XMLSTARLET_CMD=${XMLSTARLET:-xmlstarlet}
TEST_TIME=$1

URL_MOZILLA="https://www.mozilla.org/"

if [[ $BROWSER_BINARY == *"chrome"* ]]; then
  ACTIVITY="com.google.android.apps.chrome.Main"
  TOOLBAR_BOUNDS_ID="search_box_text"
  TABS_TRAY_BUTTON_BOUNDS_ID="tab_switcher_button"
fi

TAP_X=0
TAP_Y=0

calculate_tap_coords() {
    x1=$(($(echo "$1" | awk -F'[][]' '{print $2}' | awk -F',' '{print $1}')))
    x2=$(($(echo "$1" | awk -F'[][]' '{print $4}' | awk -F',' '{print $1}')))
    sum_x=$(($x1+$x2))

    y1=$(($(echo "$1" | awk -F'[][]' '{print $2}' | awk -F',' '{print $2}')))
    y2=$(($(echo "$1" | awk -F'[][]' '{print $4}' | awk -F',' '{print $2}')))
    sum_y=$(($y1+$y2))

    TAP_X=$(($sum_x/2))
    TAP_Y=$(($sum_y/2))
}

tap_at_coords(){
    adb shell input tap $TAP_X $TAP_Y
}

adb shell pm clear $BROWSER_BINARY
adb shell am start -n "$BROWSER_BINARY/$ACTIVITY"
sleep 4

if [[ $BROWSER_BINARY == *"chrome"* ]]; then
    # navigate away from the first run prompt
    adb shell uiautomator dump
    adb pull /sdcard/window_dump.xml $XML_FILE
    sleep 1

    DISMISS_BOUNDS=$($XMLSTARLET_CMD sel -t -v 'string(//node[@resource-id = "'$BROWSER_BINARY':id/signin_fre_dismiss_button"]/@bounds)' "$XML_FILE")
    sleep 1

    calculate_tap_coords $DISMISS_BOUNDS
    tap_at_coords
    sleep 2

    # navigate away from privacy notice
    adb shell uiautomator dump
    adb pull /sdcard/window_dump.xml $XML_FILE
    sleep 1

    DISMISS_BOUNDS=$($XMLSTARLET_CMD sel -t -v 'string(//node[@resource-id = "'$BROWSER_BINARY':id/ack_button"]/@bounds)' "$XML_FILE")
    sleep 1

    calculate_tap_coords $DISMISS_BOUNDS
    tap_at_coords
    sleep 1
fi

adb shell uiautomator dump
adb pull /sdcard/window_dump.xml $XML_FILE
sleep 1

# calculate toolbar coordinates
TOOLBAR_BOUNDS=$($XMLSTARLET_CMD sel -t -v '//node[@resource-id = "'$BROWSER_BINARY':id/'$TOOLBAR_BOUNDS_ID'"]/@bounds' $XML_FILE)
sleep 1

calculate_tap_coords $TOOLBAR_BOUNDS
TOOLBAR_X_COORDINATE=$TAP_X
TOOLBAR_Y_COORDINATE=$TAP_Y

# calculate tabs tray coordinates
TABS_TRAY_BUTTON_BOUNDS=$($XMLSTARLET_CMD sel -t -v '//node[@resource-id = "'$BROWSER_BINARY':id/'$TABS_TRAY_BUTTON_BOUNDS_ID'"]/@bounds' $XML_FILE)
sleep 1

calculate_tap_coords $TABS_TRAY_BUTTON_BOUNDS
TABS_TRAY_BUTTON_X_COORDINATE=$TAP_X
TABS_TRAY_BUTTON_Y_COORDINATE=$TAP_Y

adb shell input tap $TABS_TRAY_BUTTON_X_COORDINATE $TABS_TRAY_BUTTON_Y_COORDINATE
sleep 2

adb shell uiautomator dump
adb pull /sdcard/window_dump.xml $XML_FILE

# calculate new tab button coordinates
if [[ $BROWSER_BINARY == *"chrome"* ]]; then
    ADD_TAB_BUTTON_BOUNDS=$($XMLSTARLET_CMD sel -t -v '//node[@resource-id="new_tab_view_button"]/@bounds' $XML_FILE)
else
    ADD_TAB_BUTTON_BOUNDS=$($XMLSTARLET_CMD sel -t -v '//node[@content-desc="Add tab"]/@bounds' $XML_FILE)
fi
sleep 1

calculate_tap_coords $ADD_TAB_BUTTON_BOUNDS
ADD_TAB_BUTTON_X_COORDINATE=$TAP_X
ADD_TAB_BUTTON_Y_COORDINATE=$TAP_Y

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

surfingSingleSite $URL_MOZILLA

if [ "$RUN_BACKGROUND" = True ]; then
    appToBackground
fi

# at this point our system is ready, the buttons' coordinates are generated
# test starts after this line
touch $TESTING_DIR/test_start.signal
sleep $(($TEST_TIME+10)) # wait 10 mins in the background
touch $TESTING_DIR/test_end.signal
adb shell am force-stop $BROWSER_BINARY

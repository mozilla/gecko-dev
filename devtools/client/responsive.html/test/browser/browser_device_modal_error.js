/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to check that RDM can handle properly an error in the device list

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive.html/types");
const { getStr } = require("devtools/client/responsive.html/utils/l10n");

// Set a wrong URL for the device list file
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({
    set: [["devtools.devices.url", TEST_URI_ROOT + "wrong_devices_file.json"]],
  });
});

addRDMTask(TEST_URL, function* ({ ui }) {
  let { store, document } = ui.toolWindow;
  let select = document.querySelector(".viewport-device-selector");

  // Wait until the viewport has been added and the device list state indicates
  // an error
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.ERROR);

  // The device selector placeholder should be set accordingly
  let placeholder = select.options[select.selectedIndex].innerHTML;
  ok(placeholder == getStr("responsive.deviceListError"),
    "Device selector indicates an error");

  // The device selector should be disabled
  ok(select.disabled, "Device selector is disabled");
});

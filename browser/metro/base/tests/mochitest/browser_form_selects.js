// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  runTests();
}

gTests.push({
  desc: "form multi-select test 1",
  setUp: function () {
  },
  tearDown: function () {
  },
  run: function () {
    yield addTab(chromeRoot + "browser_form_selects.html");
    yield waitForCondition(function () {
      return !Browser.selectedTab.isLoading();
    });

    let win = Browser.selectedTab.browser.contentWindow;
    let tabdoc = Browser.selectedTab.browser.contentWindow.document;
    let select = tabdoc.getElementById("selectelement");

    // display the touch menu
    let promise = waitForEvent(tabdoc, "popupshown");
    sendNativeTap(select);
    yield promise;

    // tap every option
    for (let node of SelectHelperUI._listbox.childNodes) {
      sendNativeTap(node);
    }

    yield waitForCondition2(function () {
      return Browser.selectedTab.browser.contentWindow.document.getElementById("opt9").selected;
      }, "waiting for last option to select");

    // check the menu state
    for (let node of SelectHelperUI._listbox.childNodes) {
      ok(node.selected, "option is selected");
    }

    // check the underlying form state
    for (let index = 1; index < 10; index++) {
      let option = tabdoc.getElementById("opt" + index);
      ok(option.selected, "opt" + index + " form option selected");
    }

    // tap every option again
    for (let node of SelectHelperUI._listbox.childNodes) {
      sendNativeTap(node);
    }

    yield waitForCondition2(function () {
      return !Browser.selectedTab.browser.contentWindow.document.getElementById("opt9").selected;
      }, "waiting for last option to deselect");

    // check the menu state
    for (let node of SelectHelperUI._listbox.childNodes) {
      ok(!node.selected, "option is not selected");
    }

    // check the underlying form state
    for (let index = 1; index < 10; index++) {
      let option = tabdoc.getElementById("opt" + index);
      ok(!option.selected, "opt" + index + " form option not selected");
    }

  }
});


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://testing-common/AppInfo.jsm", this);
updateAppInfo({
  name: "Url Formatter Test",
  ID: "urlformattertest@test.mozilla.org",
  version: "1",
  platformVersion: "2.0",
});
var gAppInfo = getAppInfo();

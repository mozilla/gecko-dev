/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node, jest */
"use strict";

const fs = require("fs");
const path = require("path");
const { emitDom } = require("../build_dom.js");

const domdir = path.join(__dirname, "../../../dom/webidl");
const files = [
  "TestFunctions.webidl",
  "TestInterfaceJSDictionaries.webidl",
  "TestInterfaceObservableArray.webidl",
  "TestInterfaceJS.webidl",
  "TestInterfaceJSMaplikeSetlikeIterable.webidl",
  "TestUtils.webidl",
];

test("Emitting from Test*.webidl produces baseline lib.dom.d.ts", async () => {
  let webidls = files.map(f => `${domdir}/${f}`);
  let dts = await emitDom(webidls, "test_builtin.webidl");
  let baseline = path.join(__dirname, "./baselines/domtest.d.ts");
  expect(dts).toEqual(fs.readFileSync(baseline, "utf8"));
});

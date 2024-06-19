/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Aggregator } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/Aggregator.sys.mjs"
);
const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

const testData = LoginTestUtils.testData;

class MockLoginDataSource {
  lines = [];
  static match = (line, searchText) =>
    line.displayOrigin.toUpperCase().includes(searchText) ||
    line.username.toUpperCase().includes(searchText) ||
    line.password.toUpperCase().includes(searchText);

  addLines(lines) {
    for (let line of lines) {
      this.lines.push(line);
    }
  }

  *enumerateLines(searchText) {
    for (let line of this.lines) {
      if (MockLoginDataSource.match(line, searchText)) {
        yield line;
      }
    }
  }
}

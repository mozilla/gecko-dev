/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Aggregator } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/Aggregator.sys.mjs"
);

const { DataSourceBase } = ChromeUtils.importESModule(
  "resource://gre/modules/megalist/aggregator/datasources/DataSourceBase.sys.mjs"
);

const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const testData = LoginTestUtils.testData;

function mockReloadData(dataSource, lines, linePrototype) {
  dataSource.beforeReloadingDataSource();
  lines.forEach(line =>
    dataSource.addOrUpdateLine(line.record, line.id, linePrototype)
  );
  dataSource.afterReloadingDataSource();
}

function addLinesToDataSource(dataSourceBase, linePrototype) {
  const loginList = testData.loginList();
  const lines = loginList.reduce((accLines, curLogin, curIndex) => {
    accLines.push(
      { record: curLogin, id: `${curIndex}a` },
      { record: curLogin, id: `${curIndex}b` },
      { record: curLogin, id: `${curIndex}c` }
    );
    return accLines;
  }, []);
  mockReloadData(dataSourceBase, [...lines], linePrototype);
  return lines;
}

const loginMatch = searchText => record =>
  record.displayOrigin.toUpperCase().includes(searchText) ||
  record.username.toUpperCase().includes(searchText) ||
  record.password.toUpperCase().includes(searchText);

class MockLoginDataSource {
  lines = [];

  addLines(lines) {
    for (let line of lines) {
      this.lines.push(line);
    }
  }

  *enumerateLines(searchText) {
    for (let line of this.lines) {
      const matchFn = loginMatch(searchText.toUpperCase());
      if (matchFn(line.record)) {
        yield line;
      }
    }
  }
}

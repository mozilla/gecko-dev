/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const linePrototype = {
  record: { writable: true },
};

add_task(function test_dataSourceBaseAddLines() {
  const dataSourceBase = new DataSourceBase({
    refreshSingleLineOnScreen: () => {},
    refreshAllLinesOnScreen: () => {},
    setLayout: () => {},
  });
  const collator = new Intl.Collator();
  const lines = addLinesToDataSource(dataSourceBase, linePrototype);

  const expectedLines = [...lines].sort((a, b) => collator.compare(a.id, b.id));
  const expectedNumOfLines = expectedLines.length;

  Assert.equal(
    dataSourceBase.lines.length,
    expectedNumOfLines,
    "DataSourceBase inserted correct number of lines"
  );
  // lines should be sorted by id in ascending order by default
  for (let i = 0; i < expectedNumOfLines; i++) {
    Assert.equal(
      expectedLines[i].id,
      dataSourceBase.lines[i].id,
      `DataSourceBase inserted line ${dataSourceBase.lines[i].id} in correct order`
    );
  }
});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const linePrototype = {
  record: { writable: true },
};

function getExpectedLinesAndStats(lines, searchText) {
  const collator = new Intl.Collator();
  const expectedMatchingLines = lines
    .filter(line => {
      const matchFn = loginMatch(searchText);
      return matchFn(line.record);
    })
    .sort((a, b) => collator.compare(a.id, b.id));
  // expectedStats counts number of records, so divide the length
  // of the lines arrays by 3.
  const expectedStats = {
    total: lines.length / 3,
    count: expectedMatchingLines.length / 3,
  };
  return {
    expectedMatchingLines,
    expectedStats,
  };
}

function getActualLinesAndStats(dataSourceBase, searchText) {
  const actualStats = {
    total: 0,
    count: 0,
  };
  const actualMatchingLines = Array.from(
    dataSourceBase.enumerateLinesForMatchingRecords(
      searchText,
      actualStats,
      loginMatch(searchText.toUpperCase())
    )
  );
  return {
    actualMatchingLines,
    actualStats,
  };
}

add_task(function test_enumerateLinesWithSearchText() {
  const dataSourceBase = new DataSourceBase({
    refreshSingleLineOnScreen: () => {},
    refreshAllLinesOnScreen: () => {},
    setLayout: () => {},
  });

  const lines = addLinesToDataSource(dataSourceBase, linePrototype);

  for (const searchText of ["", "EXAMPLE.COM"]) {
    const { expectedMatchingLines, expectedStats } = getExpectedLinesAndStats(
      lines,
      searchText
    );
    const { actualMatchingLines, actualStats } = getActualLinesAndStats(
      dataSourceBase,
      searchText
    );
    Assert.equal(
      expectedStats.total,
      actualStats.total,
      "DataSourceBase searched through all lines"
    );
    Assert.equal(
      expectedStats.count,
      actualStats.count,
      "DataSourceBase counted correct number of matching lines"
    );
    for (let i = 0; i < expectedMatchingLines; i++) {
      Assert.equal(
        expectedMatchingLines[i].id,
        actualMatchingLines.lines[i].id,
        `DataSourceBase correctly matched line ${actualMatchingLines.lines[i].id} with the search text '${searchText}'`
      );
    }
  }
});

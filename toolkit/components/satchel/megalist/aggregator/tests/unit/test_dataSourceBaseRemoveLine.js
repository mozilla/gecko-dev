/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const linePrototype = {
  record: { writable: true },
};

add_task(function test_dataSourceBaseRemoveLine() {
  const dataSourceBase = new DataSourceBase({
    refreshSingleLineOnScreen: () => {},
    refreshAllLinesOnScreen: () => {},
    setLayout: () => {},
  });

  const lines = addLinesToDataSource(dataSourceBase, linePrototype);

  const removedLine = lines.pop();

  mockReloadData(dataSourceBase, [...lines]);

  const found = dataSourceBase.lines.find(line => line.id === removedLine.id);

  Assert.ok(
    !found,
    `DataSourceBase removed line ${removedLine.id} from its list of lines`
  );
});

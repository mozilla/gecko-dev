/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function test_dataSourceBaseExpandHeaderLine() {
  let didRefreshAllLinesOnScreen = false;
  const dataSourceBase = new DataSourceBase({
    refreshSingleLineOnScreen: () => {},
    refreshAllLinesOnScreen: () => {
      didRefreshAllLinesOnScreen = true;
    },
    setLayout: () => {},
  });

  const tooltip = {
    expand: "tooltip-expanded",
    collapse: "tooltip-collapsed",
  };

  const headerLine = dataSourceBase.createHeaderLine("test-label", tooltip);
  const previousTemplate = headerLine.template;
  headerLine.executeToggle();

  Assert.ok(
    didRefreshAllLinesOnScreen,
    "Header line refreshes all lines on screen when expanded"
  );
  Assert.equal(
    headerLine.toggleTooltip,
    tooltip.expand,
    "Header line toggles tooltip when expanded"
  );
  Assert.notEqual(
    previousTemplate,
    headerLine.template,
    "Header line toggles template when expanded"
  );
});

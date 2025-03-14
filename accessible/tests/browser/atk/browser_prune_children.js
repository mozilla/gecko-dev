/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test pruning of children from meter.
 */
addAccessibleTask(
  `
<div id="meter"
     role="meter"
     aria-valuemin="0"
     aria-valuemax="100"
     aria-valuenow="33"
     aria-valuetext="33% (Prepared)">
  <span>Prepared</span>
  <span>Shipped</span>
  <span>Delivered</span>
</div>
  `,
  async function testPruneChildren() {
    const meterChildCount = await runPython(`
      global meter
      doc = getDoc()
      meter = findByDomId(doc, "meter")
      return meter.childCount
    `);
    is(meterChildCount, 0, "Meter is pruned");

    const hasTextIface = await runPython(`
      try:
        findByDomId(doc, "meter").queryText()
      except:
        return False

      return True
    `);
    ok(!hasTextIface, "Meter should not have text interface");
  }
);

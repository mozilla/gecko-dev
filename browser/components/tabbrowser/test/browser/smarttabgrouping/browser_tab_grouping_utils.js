/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function prepareTabData(filename) {
  const ROOT_URL =
    "https://example.com/browser/browser/components/tabbrowser/test/browser/smarttabgrouping/data/";
  const rawLabels = await fetchFile(ROOT_URL, filename);
  const labels = await parseTsvStructured(rawLabels);
  const tabData = labels.map(l => {
    const label = l.smart_group_label;
    const description = `Random Description ${Math.floor(Math.random() * 100)}`;
    return {
      label,
      description,
      linkedBrowser: {
        currentURI: {
          spec: `https://example.com/${Math.floor(Math.random() * 100)}}`,
        },
      },
    };
  });
  return tabData;
}

add_task(
  async function test_default_tab_data_prep_should_not_have_description() {
    const smartTabGroupingManager = new SmartTabGroupingManager();
    const tabData = await prepareTabData("gen_set_2_labels.tsv");
    const preppedData = await smartTabGroupingManager._prepareTabData(tabData);
    for (let tab of preppedData) {
      Assert.ok(!tab.combined_text.includes(tab.description));
    }
  }
);

add_task(async function test_tab_data_prep_should_have_description_when_on() {
  const smartTabGroupingManager = new SmartTabGroupingManager();
  const tabData = await prepareTabData("gen_set_2_labels.tsv");
  const preppedData = await smartTabGroupingManager._prepareTabData(
    tabData,
    true
  );
  for (let tab of preppedData) {
    Assert.ok(tab.combined_text.includes(tab.description));
  }
});

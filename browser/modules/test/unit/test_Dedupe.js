/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Dedupe } = ChromeUtils.importESModule(
  "resource:///modules/Dedupe.sys.mjs"
);

add_task(async function test_dedupe_group() {
  let instance = new Dedupe();

  // Should remove duplicates inside the groups
  let beforeItems = [
    [1, 1, 1],
    [2, 2, 2],
    [3, 3, 3],
  ];
  let afterItems = [[1], [2], [3]];
  Assert.deepEqual(
    instance.group(...beforeItems),
    afterItems,
    "Should remove duplicates inside the groups"
  );

  // Should remove duplicates between groups, favoring earlier groups
  beforeItems = [
    [1, 2, 3],
    [2, 3, 4],
    [3, 4, 5],
  ];
  afterItems = [[1, 2, 3], [4], [5]];
  Assert.deepEqual(
    instance.group(...beforeItems),
    afterItems,
    "Should remove duplicates between groups"
  );

  // Should remove duplicates from groups of objects
  instance = new Dedupe(item => item.id);
  beforeItems = [
    [{ id: 1 }, { id: 1 }, { id: 2 }],
    [{ id: 1 }, { id: 3 }, { id: 2 }],
    [{ id: 1 }, { id: 2 }, { id: 5 }],
  ];
  afterItems = [[{ id: 1 }, { id: 2 }], [{ id: 3 }], [{ id: 5 }]];
  Assert.deepEqual(
    instance.group(...beforeItems),
    afterItems,
    "Should remove duplicates from groups of objects"
  );
});

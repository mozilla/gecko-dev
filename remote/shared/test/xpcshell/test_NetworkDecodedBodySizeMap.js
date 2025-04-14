/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { NetworkDecodedBodySizeMap } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/NetworkDecodedBodySizeMap.sys.mjs"
);

add_task(async function test_decodedBodySizeMap() {
  const map = new NetworkDecodedBodySizeMap();
  const channelId = 1;
  equal(
    map.getDecodedBodySize(channelId),
    0,
    "By default the decode body size is 0"
  );

  let expectedBodySize = 12;
  map.setDecodedBodySize(channelId, expectedBodySize);
  equal(
    map.getDecodedBodySize(channelId),
    expectedBodySize,
    "The expected body size was set"
  );

  expectedBodySize = 24;
  map.setDecodedBodySize(channelId, expectedBodySize);
  equal(
    map.getDecodedBodySize(channelId),
    expectedBodySize,
    "The expected body size was updated"
  );

  map.delete(channelId);
  equal(
    map.getDecodedBodySize(channelId),
    0,
    "After deleting the channel entry, the size is back to 0"
  );

  map.destroy();
});

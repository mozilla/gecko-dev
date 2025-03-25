/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { NetworkDecodedBodySizeMap } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/NetworkDecodedBodySizeMap.sys.mjs"
);

add_task(async function test_get_set_delete_decodedBodySize() {
  const map = new NetworkDecodedBodySizeMap();
  const channelId = 1;
  let onDecodedBodySize = map.getDecodedBodySize(channelId);

  ok(
    !(await hasPromiseResolved(onDecodedBodySize)),
    "onDecodedBodySize has not resolved yet"
  );

  const expectedBodySize = 12;
  map.setDecodedBodySize(channelId, expectedBodySize);
  ok(
    await hasPromiseResolved(onDecodedBodySize),
    "onDecodedBodySize has resolved"
  );

  equal(
    await onDecodedBodySize,
    expectedBodySize,
    "decodedBodySize has the expected value"
  );

  // Check that calling getDecodedBodySize for the same channel id resolves
  // immediately
  onDecodedBodySize = map.getDecodedBodySize(channelId);
  ok(
    await hasPromiseResolved(onDecodedBodySize),
    "onDecodedBodySize has resolved"
  );
  equal(
    await onDecodedBodySize,
    expectedBodySize,
    "decodedBodySize has the expected value"
  );

  // Delete the entry for channelId and check that the promise no longer
  // resolves.
  map.delete(channelId);
  onDecodedBodySize = map.getDecodedBodySize(channelId);
  ok(
    !(await hasPromiseResolved(onDecodedBodySize)),
    "onDecodedBodySize has not resolved yet"
  );

  map.destroy();
});

add_task(async function test_set_other_channel() {
  const map = new NetworkDecodedBodySizeMap();
  const channelId = 1;
  const otherChannelId = 2;
  const onDecodedBodySize = map.getDecodedBodySize(channelId);

  ok(
    !(await hasPromiseResolved(onDecodedBodySize)),
    "onDecodedBodySize has not resolved yet"
  );

  map.setDecodedBodySize(otherChannelId, 12);

  ok(
    !(await hasPromiseResolved(onDecodedBodySize)),
    "onDecodedBodySize has still not resolved"
  );

  map.destroy();
});

add_task(async function test_get_twice() {
  const map = new NetworkDecodedBodySizeMap();
  const channelId = 1;
  const onDecodedBodySize1 = map.getDecodedBodySize(channelId);

  ok(
    !(await hasPromiseResolved(onDecodedBodySize1)),
    "onDecodedBodySize1 has not resolved yet"
  );

  // Call getDecodedBodySize another time to check we still wait for the promise
  const onDecodedBodySize2 = map.getDecodedBodySize(channelId);
  ok(
    !(await hasPromiseResolved(onDecodedBodySize2)),
    "onDecodedBodySize2 has not resolved yet"
  );

  // Set the body size and check that both promises resolved the same value.
  const expectedBodySize = 12;
  map.setDecodedBodySize(channelId, expectedBodySize);
  ok(
    await hasPromiseResolved(onDecodedBodySize1),
    "onDecodedBodySize1 has resolved"
  );
  ok(
    await hasPromiseResolved(onDecodedBodySize2),
    "onDecodedBodySize2 has resolved"
  );

  equal(
    await onDecodedBodySize1,
    expectedBodySize,
    "decodedBodySize has the expected value"
  );
  equal(
    await onDecodedBodySize2,
    expectedBodySize,
    "decodedBodySize has the expected value"
  );

  // Set another body size and check that further calls to getDecodedBodySize
  // resolve the new value.
  const otherBodySize = 42;
  map.setDecodedBodySize(channelId, otherBodySize);
  equal(
    await map.getDecodedBodySize(channelId),
    otherBodySize,
    "decodedBodySize has the expected value"
  );

  map.destroy();
});

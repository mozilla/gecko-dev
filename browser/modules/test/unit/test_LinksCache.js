/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { LinksCache } = ChromeUtils.importESModule(
  "resource:///modules/LinksCache.sys.mjs"
);

add_task(async function test_LinksCache_throws_on_failing_request() {
  const cache = new LinksCache();

  let rejected = false;
  try {
    await cache.request();
  } catch (error) {
    rejected = true;
  }

  Assert.ok(rejected, "The request should throw an error when failing.");
});

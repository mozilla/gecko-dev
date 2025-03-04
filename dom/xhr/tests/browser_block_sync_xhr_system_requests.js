/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_block_sync_xhr_requests() {
  await SpecialPowers.pushPrefEnv({
    set: [["network.xhr.block_sync_system_requests", true]],
  });

  Assert.throws(
    () => {
      let xhr = new XMLHttpRequest();
      // false means a synchronous request
      xhr.open("GET", "https://example.com", false);
      xhr.send();
    },
    /NetworkError/,
    "Sync XHR coming from system requests should be blocked"
  );
});

add_task(async function test_not_block_sync_xhr_requests() {
  await SpecialPowers.pushPrefEnv({
    set: [["network.xhr.block_sync_system_requests", false]],
  });

  let xhr = new XMLHttpRequest();
  xhr.open("GET", "https://example.com", false);
  xhr.send();

  is(
    xhr.status,
    200,
    "Sync XHR coming from system requests should be allowed when pref is false"
  );
});

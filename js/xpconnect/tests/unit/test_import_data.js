/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let caught = false;
  try {
    ChromeUtils.importESModule("data:text/javascript,");
  } catch (e) {
    caught = true;
    Assert.equal(e.message, "System modules must be loaded from a trusted scheme");
  }
  Assert.ok(caught);
});

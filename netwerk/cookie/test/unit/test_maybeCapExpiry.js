"use strict";

add_task(function test_maybeCapExpiry() {
  const CAP_PREF = "network.cookie.maxageCap";
  Services.prefs.setIntPref(CAP_PREF, 10); // 10 seconds cap

  let now = Date.now();
  let expiry = now + 60 * 1000; // 60 seconds in future
  let capped = Services.cookies.maybeCapExpiry(expiry);

  Assert.ok(
    capped <= now + 20 * 1000,
    "expiry should be capped to about now + cap"
  );
  Assert.ok(capped <= expiry, "result should not exceed original expiry");

  // Expiry below cap should be returned unchanged
  let smallExpiry = now + 1000; // 1 seconds
  let res = Services.cookies.maybeCapExpiry(smallExpiry);
  Assert.equal(res, smallExpiry, "expiry below cap unchanged");

  Services.prefs.clearUserPref(CAP_PREF);
});

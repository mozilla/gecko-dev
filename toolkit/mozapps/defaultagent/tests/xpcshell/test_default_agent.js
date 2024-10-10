/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const defaultAgent = Cc["@mozilla.org/default-agent;1"].getService(
  Ci.nsIDefaultAgent
);

add_task(function test_secondsSinceLastAppRun() {
  try {
    const secondsSinceAppRan = defaultAgent.secondsSinceLastAppRun();
    Assert.greaterOrEqual(
      secondsSinceAppRan,
      0,
      "If we get a number back, it should be non-negative"
    );
  } catch (err) {
    Assert.equal(
      err.result,
      Cr.NS_ERROR_NOT_AVAILABLE,
      "Otherwise, NS_ERROR_NOT_AVAILABLE means that no value was set"
    );
  }
});

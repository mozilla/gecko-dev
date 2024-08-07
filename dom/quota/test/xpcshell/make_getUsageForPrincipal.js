/*
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
*/

async function testSteps() {
  const principal = getPrincipal("http://localhost");

  const name = "test_getUsageForPrincipal.js";

  info("Opening database");

  let request = indexedDB.openForPrincipal(principal, name);
  await openDBRequestSucceeded(request);
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function nonexistent() {
  await set("a.com", "foo", 1);
  await setGlobal("foo", 2);

  await new Promise(resolve =>
    cps.removeByName("bogus", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);
  await reset();
});

add_task(async function names() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);
  await set("b.com", "bar", 6);

  await new Promise(resolve =>
    cps.removeByName("foo", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "bar", 2],
    [null, "bar", 4],
    ["b.com", "bar", 6],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], undefined);
  await getOK(["b.com", "bar"], 6);
  await reset();
});

add_task(async function privateBrowsing() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);
  await set("b.com", "bar", 6);

  let context = privateLoadContext;
  await set("a.com", "foo", 7, context);
  await setGlobal("foo", 8, context);
  await set("b.com", "bar", 9, context);
  await new Promise(resolve =>
    cps.removeByName("bar", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 3],
    ["b.com", "foo", 5],
  ]);
  await getOK(["a.com", "foo", context], 7);
  await getOK(["a.com", "bar", context], undefined);
  await getGlobalOK(["foo", context], 8);
  await getGlobalOK(["bar", context], undefined);
  await getOK(["b.com", "foo", context], 5);
  await getOK(["b.com", "bar", context], undefined);

  await getOK(["a.com", "foo"], 1);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], undefined);
  await getOK(["b.com", "foo"], 5);
  await getOK(["b.com", "bar"], undefined);
  await reset();
});

/**
 * Tests that when clearing data for normal browsing, private browsing is not
 * affected and vice versa.
 */
add_task(async function privateBrowsingIsolatedRemoval() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await setGlobal("qux", 5);
  await set("b.com", "foo", 6);
  await set("b.com", "bar", 7);

  await set("a.com", "foo", 8, privateLoadContext);
  await set("b.com", "foo", 9, privateLoadContext);
  await set("b.com", "bar", 10, privateLoadContext);
  await setGlobal("foo", 11, privateLoadContext);

  info("Clear 'foo' for non PBM.");
  await new Promise(resolve =>
    cps.removeByName("foo", loadContext, makeCallback(resolve))
  );

  info("For 'foo' only the PBM entries should remain.");
  await getOK(["a.com", "foo", loadContext], undefined);
  await getOK(["a.com", "bar", loadContext], 2);
  await getGlobalOK(["foo", loadContext], undefined);
  await getGlobalOK(["bar", loadContext], 4);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], undefined);
  await getOK(["b.com", "bar", loadContext], 7);

  await getOK(["a.com", "foo", privateLoadContext], 8);
  await getOK(["b.com", "foo", privateLoadContext], 9);
  await getOK(["b.com", "bar", privateLoadContext], 10);
  await getGlobalOK(["foo", privateLoadContext], 11);

  info("Clear 'bar' for PBM.");
  await new Promise(resolve =>
    cps.removeByName("bar", privateLoadContext, makeCallback(resolve))
  );

  info("For 'bar' only the non PBM entries should remain.");
  await getOK(["a.com", "foo", loadContext], undefined);
  await getOK(["a.com", "bar", loadContext], 2);
  await getGlobalOK(["foo", loadContext], undefined);
  await getGlobalOK(["bar", loadContext], 4);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], undefined);
  await getOK(["b.com", "bar", loadContext], 7);

  await getOK(["a.com", "foo", privateLoadContext], 8);
  await getOK(["b.com", "foo", privateLoadContext], 9);
  // This still returns an entry because even if a PBM load context is passed we
  // will fall back to non PBM entry with the same key.
  await getOK(["b.com", "bar", privateLoadContext], 7);
  await getGlobalOK(["foo", privateLoadContext], 11);

  info("Clear 'foo' for PBM.");
  await new Promise(resolve =>
    cps.removeByName("foo", privateLoadContext, makeCallback(resolve))
  );

  info("All 'foo' entries should have been cleared.");
  await getOK(["a.com", "foo", loadContext], undefined);
  await getOK(["a.com", "bar", loadContext], 2);
  await getGlobalOK(["foo", loadContext], undefined);
  await getGlobalOK(["bar", loadContext], 4);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], undefined);
  await getOK(["b.com", "bar", loadContext], 7);

  await getOK(["a.com", "foo", privateLoadContext], undefined);
  await getOK(["b.com", "foo", privateLoadContext], undefined);
  // This still returns an entry because even if a PBM load context is passed we
  // will fall back to non PBM entry with the same key.
  await getOK(["b.com", "bar", privateLoadContext], 7);
  await getGlobalOK(["foo", privateLoadContext], undefined);

  info("Clear 'bar' for non PBM.");
  await new Promise(resolve =>
    cps.removeByName("bar", loadContext, makeCallback(resolve))
  );

  info("All 'bar' and 'foo' entries should have been cleared.");
  await getOK(["a.com", "foo", loadContext], undefined);
  await getOK(["a.com", "bar", loadContext], undefined);
  await getGlobalOK(["foo", loadContext], undefined);
  await getGlobalOK(["bar", loadContext], undefined);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], undefined);
  await getOK(["b.com", "bar", loadContext], undefined);

  await getOK(["a.com", "foo", privateLoadContext], undefined);
  await getOK(["b.com", "foo", privateLoadContext], undefined);
  await getOK(["b.com", "bar", privateLoadContext], undefined);
  await getGlobalOK(["foo", privateLoadContext], undefined);

  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.removeByName("", null));
  do_check_throws(() => cps.removeByName(null, null));
  do_check_throws(() => cps.removeByName("foo", null, "bogus"));
  await reset();
});

add_task(async function invalidateCache() {
  await set("a.com", "foo", 1);
  await set("b.com", "foo", 2);
  getCachedOK(["a.com", "foo"], true, 1);
  getCachedOK(["b.com", "foo"], true, 2);
  cps.removeByName("foo", null, makeCallback());
  getCachedOK(["a.com", "foo"], false);
  getCachedOK(["b.com", "foo"], false);
  await reset();
});

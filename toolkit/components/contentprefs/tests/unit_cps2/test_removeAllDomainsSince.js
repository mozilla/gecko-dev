/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function resetBeforeTests() {
  await reset();
});

add_task(async function nonexistent() {
  await setGlobal("foo", 1);
  await new Promise(resolve =>
    cps.removeAllDomainsSince(0, null, makeCallback(resolve))
  );
  await getGlobalOK(["foo"], 1);
  await reset();
});

add_task(async function domainsAll() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);
  await set("b.com", "bar", 6);

  await new Promise(resolve =>
    cps.removeAllDomainsSince(0, null, makeCallback(resolve))
  );
  await dbOK([
    [null, "foo", 3],
    [null, "bar", 4],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], undefined);
  await getOK(["b.com", "bar"], undefined);
  await reset();
});

add_task(async function domainsWithDate() {
  await setWithDate("a.com", "foobar", 0, 0);
  await setWithDate("a.com", "foo", 1, 1000);
  await setWithDate("a.com", "bar", 2, 4000);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await setWithDate("b.com", "foo", 5, 2000);
  await setWithDate("b.com", "bar", 6, 3000);
  await setWithDate("b.com", "foobar", 7, 1000);

  await new Promise(resolve =>
    cps.removeAllDomainsSince(2000, null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foobar", 0],
    ["a.com", "foo", 1],
    [null, "foo", 3],
    [null, "bar", 4],
    ["b.com", "foobar", 7],
  ]);
  await reset();
});

add_task(async function privateBrowsing() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await set("b.com", "foo", 5);

  let context = privateLoadContext;
  await set("a.com", "foo", 6, context);
  await setGlobal("foo", 7, context);
  await new Promise(resolve =>
    // Passing context=null clears both normal and private browsing data.
    cps.removeAllDomainsSince(0, null, makeCallback(resolve))
  );
  await dbOK([
    [null, "foo", 3],
    [null, "bar", 4],
  ]);
  await getOK(["a.com", "foo", context], undefined);
  await getOK(["a.com", "bar", context], undefined);
  await getGlobalOK(["foo", context], 7);
  await getGlobalOK(["bar", context], 4);
  await getOK(["b.com", "foo", context], undefined);

  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);
  await getOK(["b.com", "foo"], undefined);
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
  await setGlobal("foo", 10, privateLoadContext);

  info("Clear all PBM data.");
  await new Promise(resolve =>
    cps.removeAllDomainsSince(0, privateLoadContext, makeCallback(resolve))
  );

  await getOK(["a.com", "foo", loadContext], 1);
  await getOK(["a.com", "bar", loadContext], 2);
  await getGlobalOK(["foo", loadContext], 3);
  await getGlobalOK(["bar", loadContext], 4);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], 6);
  await getOK(["b.com", "bar", loadContext], 7);

  // This still returns an entry because even if a PBM load context is passed we
  // will fall back to non PBM entries with the same key.
  await getOK(["a.com", "foo", privateLoadContext], 1);
  await getOK(["b.com", "foo", privateLoadContext], 6);
  await getGlobalOK(["foo", privateLoadContext], 10);

  info("Clear all non PBM data.");
  await new Promise(resolve =>
    cps.removeAllDomainsSince(0, loadContext, makeCallback(resolve))
  );

  info("Should have cleared all domain keyed entries");
  await getOK(["a.com", "foo", loadContext], undefined);
  await getOK(["a.com", "bar", loadContext], undefined);
  await getGlobalOK(["foo", loadContext], 3);
  await getGlobalOK(["bar", loadContext], 4);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], undefined);
  await getOK(["b.com", "bar", loadContext], undefined);

  await getOK(["a.com", "foo", privateLoadContext], undefined);
  await getOK(["b.com", "foo", privateLoadContext], undefined);
  await getGlobalOK(["foo", privateLoadContext], 10);

  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.removeAllDomainsSince(null, "bogus"));
  await reset();
});

add_task(async function invalidateCache() {
  await setWithDate("a.com", "foobar", 0, 0);
  await setWithDate("a.com", "foo", 1, 1000);
  await setWithDate("a.com", "bar", 2, 4000);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await setWithDate("b.com", "foo", 5, 2000);
  await setWithDate("b.com", "bar", 6, 3000);
  await setWithDate("b.com", "foobar", 7, 1000);
  let clearPromise = new Promise(resolve => {
    cps.removeAllDomainsSince(0, null, makeCallback(resolve));
  });
  getCachedOK(["a.com", "foobar"], false);
  getCachedOK(["a.com", "foo"], false);
  getCachedOK(["a.com", "bar"], false);
  getCachedGlobalOK(["foo"], true, 3);
  getCachedGlobalOK(["bar"], true, 4);
  getCachedOK(["b.com", "foo"], false);
  getCachedOK(["b.com", "bar"], false);
  getCachedOK(["b.com", "foobar"], false);
  await clearPromise;
  await reset();
});

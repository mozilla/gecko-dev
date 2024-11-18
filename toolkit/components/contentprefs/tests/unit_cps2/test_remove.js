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
    cps.removeByDomainAndName("a.com", "bogus", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);

  await new Promise(resolve =>
    cps.removeBySubdomainAndName("a.com", "bogus", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);

  await new Promise(resolve =>
    cps.removeGlobal("bogus", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);

  await new Promise(resolve =>
    cps.removeByDomainAndName("bogus", "bogus", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "foo", 1],
    [null, "foo", 2],
  ]);
  await getOK(["a.com", "foo"], 1);
  await getGlobalOK(["foo"], 2);
  await reset();
});

add_task(async function isomorphicDomains() {
  await set("a.com", "foo", 1);
  await new Promise(resolve =>
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  await dbOK([]);
  await getOK(["a.com", "foo"], undefined);

  await set("a.com", "foo", 2);
  await new Promise(resolve =>
    cps.removeByDomainAndName(
      "http://a.com/huh",
      "foo",
      null,
      makeCallback(resolve)
    )
  );
  await dbOK([]);
  await getOK(["a.com", "foo"], undefined);
  await reset();
});

add_task(async function names() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);

  await new Promise(resolve =>
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "bar", 2],
    [null, "foo", 3],
    [null, "bar", 4],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], 3);
  await getGlobalOK(["bar"], 4);

  await new Promise(resolve =>
    cps.removeGlobal("foo", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "bar", 2],
    [null, "bar", 4],
  ]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], 4);

  await new Promise(resolve =>
    cps.removeByDomainAndName("a.com", "bar", null, makeCallback(resolve))
  );
  await dbOK([[null, "bar", 4]]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], 4);

  await new Promise(resolve =>
    cps.removeGlobal("bar", null, makeCallback(resolve))
  );
  await dbOK([]);
  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], undefined);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], undefined);
  await reset();
});

add_task(async function subdomains() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  await new Promise(resolve =>
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  await dbOK([["b.a.com", "foo", 2]]);
  await getSubdomainsOK(["a.com", "foo"], [["b.a.com", 2]]);
  await getSubdomainsOK(["b.a.com", "foo"], [["b.a.com", 2]]);

  await set("a.com", "foo", 3);
  await new Promise(resolve =>
    cps.removeBySubdomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  await dbOK([]);
  await getSubdomainsOK(["a.com", "foo"], []);
  await getSubdomainsOK(["b.a.com", "foo"], []);

  await set("a.com", "foo", 4);
  await set("b.a.com", "foo", 5);
  await new Promise(resolve =>
    cps.removeByDomainAndName("b.a.com", "foo", null, makeCallback(resolve))
  );
  await dbOK([["a.com", "foo", 4]]);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 4]]);
  await getSubdomainsOK(["b.a.com", "foo"], []);

  await set("b.a.com", "foo", 6);
  await new Promise(resolve =>
    cps.removeBySubdomainAndName("b.a.com", "foo", null, makeCallback(resolve))
  );
  await dbOK([["a.com", "foo", 4]]);
  await getSubdomainsOK(["a.com", "foo"], [["a.com", 4]]);
  await getSubdomainsOK(["b.a.com", "foo"], []);
  await reset();
});

add_task(async function privateBrowsing() {
  await set("a.com", "foo", 1);
  await set("a.com", "bar", 2);
  await setGlobal("foo", 3);
  await setGlobal("bar", 4);
  await setGlobal("qux", 5);
  await set("b.com", "foo", 6);
  await set("b.com", "bar", 7);

  let context = privateLoadContext;
  await set("a.com", "foo", 8, context);
  await setGlobal("foo", 9, context);
  await new Promise(resolve =>
    // Passing context=null clears both normal and private browsing data.
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback(resolve))
  );
  await new Promise(resolve =>
    cps.removeGlobal("foo", null, makeCallback(resolve))
  );
  await new Promise(resolve =>
    cps.removeGlobal("qux", null, makeCallback(resolve))
  );
  await new Promise(resolve =>
    // Passing context=null clears both normal and private browsing data.
    cps.removeByDomainAndName("b.com", "foo", null, makeCallback(resolve))
  );
  await dbOK([
    ["a.com", "bar", 2],
    [null, "bar", 4],
    ["b.com", "bar", 7],
  ]);
  await getOK(["a.com", "foo", context], undefined);
  await getOK(["a.com", "bar", context], 2);
  await getGlobalOK(["foo", context], undefined);
  await getGlobalOK(["bar", context], 4);
  await getGlobalOK(["qux", context], undefined);
  await getOK(["b.com", "foo", context], undefined);
  await getOK(["b.com", "bar", context], 7);

  await getOK(["a.com", "foo"], undefined);
  await getOK(["a.com", "bar"], 2);
  await getGlobalOK(["foo"], undefined);
  await getGlobalOK(["bar"], 4);
  await getGlobalOK(["qux"], undefined);
  await getOK(["b.com", "foo"], undefined);
  await getOK(["b.com", "bar"], 7);
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

  info("For a.com only clear the normal browsing entry.");
  await new Promise(resolve =>
    cps.removeByDomainAndName(
      "a.com",
      "foo",
      loadContext,
      makeCallback(resolve)
    )
  );

  info("For a.com, 'foo' only the PBM entry should remain.");
  await getOK(["a.com", "foo", loadContext], undefined);
  await getOK(["a.com", "bar", loadContext], 2);
  await getGlobalOK(["foo", loadContext], 3);
  await getGlobalOK(["bar", loadContext], 4);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], 6);
  await getOK(["b.com", "bar", loadContext], 7);

  await getOK(["a.com", "foo", privateLoadContext], 8);
  await getOK(["b.com", "foo", privateLoadContext], 9);
  await getGlobalOK(["foo", privateLoadContext], 10);

  info("For b.com only clear PBM entry.");
  await new Promise(resolve =>
    cps.removeByDomainAndName(
      "b.com",
      "foo",
      privateLoadContext,
      makeCallback(resolve)
    )
  );

  info("For b.com, 'foo' only the non PBM entry should remain.");
  await getOK(["a.com", "foo", loadContext], undefined);
  await getOK(["a.com", "bar", loadContext], 2);
  await getGlobalOK(["foo", loadContext], 3);
  await getGlobalOK(["bar", loadContext], 4);
  await getGlobalOK(["qux", loadContext], 5);
  await getOK(["b.com", "foo", loadContext], 6);
  await getOK(["b.com", "bar", loadContext], 7);

  await getOK(["a.com", "foo", privateLoadContext], 8);
  // This still returns an entry because even if a PBM load context is passed we
  // will fall back to non PBM entries with the same key.
  await getOK(["b.com", "foo", privateLoadContext], 6);
  await getGlobalOK(["foo", privateLoadContext], 10);

  await reset();
});

add_task(async function erroneous() {
  do_check_throws(() => cps.removeByDomainAndName(null, "foo", null));
  do_check_throws(() => cps.removeByDomainAndName("", "foo", null));
  do_check_throws(() =>
    cps.removeByDomainAndName("a.com", "foo", null, "bogus")
  );
  do_check_throws(() => cps.removeBySubdomainAndName(null, "foo", null));
  do_check_throws(() => cps.removeBySubdomainAndName("", "foo", null));
  do_check_throws(() =>
    cps.removeBySubdomainAndName("a.com", "foo", null, "bogus")
  );
  do_check_throws(() => cps.removeGlobal("", null));
  do_check_throws(() => cps.removeGlobal(null, null));
  do_check_throws(() => cps.removeGlobal("foo", null, "bogus"));
  await reset();
});

add_task(async function removeByDomainAndName_invalidateCache() {
  await set("a.com", "foo", 1);
  getCachedOK(["a.com", "foo"], true, 1);
  let promiseRemoved = new Promise(resolve => {
    cps.removeByDomainAndName("a.com", "foo", null, makeCallback(resolve));
  });
  getCachedOK(["a.com", "foo"], false);
  await promiseRemoved;
  await reset();
});

add_task(async function removeBySubdomainAndName_invalidateCache() {
  await set("a.com", "foo", 1);
  await set("b.a.com", "foo", 2);
  getCachedSubdomainsOK(
    ["a.com", "foo"],
    [
      ["a.com", 1],
      ["b.a.com", 2],
    ]
  );
  let promiseRemoved = new Promise(resolve => {
    cps.removeBySubdomainAndName("a.com", "foo", null, makeCallback(resolve));
  });
  getCachedSubdomainsOK(["a.com", "foo"], []);
  await promiseRemoved;
  await reset();
});

add_task(async function removeGlobal_invalidateCache() {
  await setGlobal("foo", 1);
  getCachedGlobalOK(["foo"], true, 1);
  let promiseRemoved = new Promise(resolve => {
    cps.removeGlobal("foo", null, makeCallback(resolve));
  });
  getCachedGlobalOK(["foo"], false);
  await promiseRemoved;
  await reset();
});

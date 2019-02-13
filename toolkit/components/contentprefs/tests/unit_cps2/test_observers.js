/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  runAsyncTests(tests);
}

let tests = [

  function observerForName_set() {
    yield set("a.com", "foo", 1);
    let args = yield on("Set", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo", 1]]);
    observerArgsOK(args.null, [["a.com", "foo", 1]]);
    observerArgsOK(args.bar, []);

    yield setGlobal("foo", 2);
    args = yield on("Set", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [[null, "foo", 2]]);
    observerArgsOK(args.null, [[null, "foo", 2]]);
    observerArgsOK(args.bar, []);
  },

  function observerForName_remove() {
    yield set("a.com", "foo", 1);
    yield setGlobal("foo", 2);

    yield cps.removeByDomainAndName("a.com", "bogus", null, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);

    yield cps.removeByDomainAndName("a.com", "foo", null, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"]]);
    observerArgsOK(args.bar, []);

    yield cps.removeGlobal("foo", null, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [[null, "foo"]]);
    observerArgsOK(args.null, [[null, "foo"]]);
    observerArgsOK(args.bar, []);
  },

  function observerForName_removeByDomain() {
    yield set("a.com", "foo", 1);
    yield set("b.a.com", "bar", 2);
    yield setGlobal("foo", 3);

    yield cps.removeByDomain("bogus", null, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);

    yield cps.removeBySubdomain("a.com", null, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"], ["b.a.com", "bar"]]);
    observerArgsOK(args.bar, [["b.a.com", "bar"]]);

    yield cps.removeAllGlobals(null, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [[null, "foo"]]);
    observerArgsOK(args.null, [[null, "foo"]]);
    observerArgsOK(args.bar, []);
  },

  function observerForName_removeAllDomainsSince() {
    yield setWithDate("a.com", "foo", 1, 100);
    yield setWithDate("b.com", "foo", 2, 200);
    yield setWithDate("c.com", "foo", 3, 300);

    yield setWithDate("a.com", "bar", 1, 000);
    yield setWithDate("b.com", "bar", 2, 100);
    yield setWithDate("c.com", "bar", 3, 200);
    yield setGlobal("foo", 2);

    yield cps.removeAllDomainsSince(200, null, makeCallback());

    let args = yield on("Removed", ["foo", "bar", null]);

    observerArgsOK(args.foo, [["b.com", "foo"], ["c.com", "foo"]]);
    observerArgsOK(args.bar, [["c.com", "bar"]]);
    observerArgsOK(args.null, [["b.com", "foo"], ["c.com", "bar"], ["c.com", "foo"]]);
  },

  function observerForName_removeAllDomains() {
    yield set("a.com", "foo", 1);
    yield setGlobal("foo", 2);
    yield set("b.com", "bar", 3);

    yield cps.removeAllDomains(null, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"], ["b.com", "bar"]]);
    observerArgsOK(args.bar, [["b.com", "bar"]]);
  },

  function observerForName_removeByName() {
    yield set("a.com", "foo", 1);
    yield set("a.com", "bar", 2);
    yield setGlobal("foo", 3);

    yield cps.removeByName("bogus", null, makeCallback());
    let args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);

    yield cps.removeByName("foo", null, makeCallback());
    args = yield on("Removed", ["foo", null, "bar"]);
    observerArgsOK(args.foo, [["a.com", "foo"], [null, "foo"]]);
    observerArgsOK(args.null, [["a.com", "foo"], [null, "foo"]]);
    observerArgsOK(args.bar, []);
  },

  function removeObserverForName() {
    let args = yield on("Set", ["foo", null, "bar"], true);

    cps.removeObserverForName("foo", args.foo.observer);
    yield set("a.com", "foo", 1);
    yield wait();
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, [["a.com", "foo", 1]]);
    observerArgsOK(args.bar, []);
    args.reset();

    cps.removeObserverForName(null, args.null.observer);
    yield set("a.com", "foo", 2);
    yield wait();
    observerArgsOK(args.foo, []);
    observerArgsOK(args.null, []);
    observerArgsOK(args.bar, []);
    args.reset();
  },
];

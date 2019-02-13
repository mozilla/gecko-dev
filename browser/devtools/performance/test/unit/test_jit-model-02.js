/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that JITOptimizations create OptimizationSites, and the underlying
 * OptimizationSites methods work as expected.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  let { JITOptimizations, OptimizationSite } = devtools.require("devtools/performance/jit");

  let rawSites = [];
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite1);
  rawSites.push(gRawSite1);
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite3);

  let jit = new JITOptimizations(rawSites, gStringTable.stringTable);
  let sites = jit.optimizationSites;

  let [first, second, third] = sites;

  /* hasSuccessfulOutcome */
  equal(first.hasSuccessfulOutcome(), false, "optSite.hasSuccessfulOutcome() returns expected (1)");
  equal(second.hasSuccessfulOutcome(), true, "optSite.hasSuccessfulOutcome() returns expected (2)");
  equal(third.hasSuccessfulOutcome(), true, "optSite.hasSuccessfulOutcome() returns expected (3)");

  /* getAttempts */
  equal(first.getAttempts().length, 2, "optSite.getAttempts() has the correct amount of attempts (1)");
  equal(second.getAttempts().length, 5, "optSite.getAttempts() has the correct amount of attempts (2)");
  equal(third.getAttempts().length, 3, "optSite.getAttempts() has the correct amount of attempts (3)");

  /* getIonTypes */
  equal(first.getIonTypes().length, 1, "optSite.getIonTypes() has the correct amount of IonTypes (1)");
  equal(second.getIonTypes().length, 2, "optSite.getIonTypes() has the correct amount of IonTypes (2)");
  equal(third.getIonTypes().length, 1, "optSite.getIonTypes() has the correct amount of IonTypes (3)");
});


let gStringTable = new RecordingUtils.UniqueStrings();

function uniqStr(s) {
  return gStringTable.getOrAddStringIndex(s);
}

let gRawSite1 = {
  line: 12,
  column: 2,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }, {
      keyedBy: uniqStr("constructor"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }]
  }, {
    mirType: uniqStr("Int32"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("primitive"),
      location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Inlined"), uniqStr("SomeGetter3")]
    ]
  }
};

let gRawSite2 = {
  line: 34,
  types: [{
    mirType: uniqStr("Int32"),
    site: uniqStr("Receiver")
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")]
    ]
  }
};

let gRawSite3 = {
  line: 78,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }, {
      keyedBy: uniqStr("primitive"),
      location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("GenericSuccess"), uniqStr("SomeGetter3")]
    ]
  }
};

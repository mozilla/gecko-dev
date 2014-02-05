/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {}).Promise;
const EventEmitter = Cu.import("resource:///modules/devtools/shared/event-emitter.js", {}).EventEmitter;

function test() {
  waitForExplicitFinish();

  testEmitter();
  testEmitter({});

  Task.spawn(testPromise).then(null, ok.bind(null, false)).then(finish);
}

function testEmitter(aObject) {
  let emitter;

  if (aObject) {
    emitter = aObject;
    EventEmitter.decorate(emitter);
  } else {
    emitter = new EventEmitter();
  }

  ok(emitter, "We have an event emitter");

  emitter.on("next", next);
  emitter.emit("next", "abc", "def");

  let beenHere1 = false;
  function next(eventName, str1, str2) {
    is(eventName, "next", "Got event");
    is(str1, "abc", "Argument 1 is correct");
    is(str2, "def", "Argument 2 is correct");

    ok(!beenHere1, "first time in next callback");
    beenHere1 = true;

    emitter.off("next", next);

    emitter.emit("next");

    emitter.once("onlyonce", onlyOnce);

    emitter.emit("onlyonce");
    emitter.emit("onlyonce");
  }

  let beenHere2 = false;
  function onlyOnce() {
    ok(!beenHere2, "\"once\" listner has been called once");
    beenHere2 = true;
    emitter.emit("onlyonce");

    killItWhileEmitting();
  }

  function killItWhileEmitting() {
    function c1() {
      ok(true, "c1 called");
    }
    function c2() {
      ok(true, "c2 called");
      emitter.off("tick", c3);
    }
    function c3() {
      ok(false, "c3 should not be called");
    }
    function c4() {
      ok(true, "c4 called");
    }

    emitter.on("tick", c1);
    emitter.on("tick", c2);
    emitter.on("tick", c3);
    emitter.on("tick", c4);

    emitter.emit("tick");

    offAfterOnce();
  }

  function offAfterOnce() {
    let enteredC1 = false;

    function c1() {
      enteredC1 = true;
    }

    emitter.once("oao", c1);
    emitter.off("oao", c1);

    emitter.emit("oao");

    ok(!enteredC1, "c1 should not be called");
  }
}

function testPromise() {
  let emitter = new EventEmitter();
  let p = emitter.once("thing");

  // Check that the promise is only resolved once event though we
  // emit("thing") more than once
  let firstCallbackCalled = false;
  let check1 = p.then(arg => {
    is(firstCallbackCalled, false, "first callback called only once");
    firstCallbackCalled = true;
    is(arg, "happened", "correct arg in promise");
    return "rval from c1";
  });

  emitter.emit("thing", "happened", "ignored");

  // Check that the promise is resolved asynchronously
  let secondCallbackCalled = false;
  let check2 = p.then(arg => {
    ok(true, "second callback called");
    is(arg, "happened", "correct arg in promise");
    secondCallbackCalled = true;
    is(arg, "happened", "correct arg in promise (a second time)");
    return "rval from c2";
  });

  // Shouldn't call any of the above listeners
  emitter.emit("thing", "trashinate");

  // Check that we can still separate events with different names
  // and that it works with no parameters
  let pfoo = emitter.once("foo");
  let pbar = emitter.once("bar");

  let check3 = pfoo.then(arg => {
    ok(arg === undefined, "no arg for foo event");
    return "rval from c3";
  });

  pbar.then(() => {
    ok(false, "pbar should not be called");
  });

  emitter.emit("foo");

  is(secondCallbackCalled, false, "second callback not called yet");

  return promise.all([ check1, check2, check3 ]).then(args => {
    is(args[0], "rval from c1", "callback 1 done good");
    is(args[1], "rval from c2", "callback 2 done good");
    is(args[2], "rval from c3", "callback 3 done good");
  });
}

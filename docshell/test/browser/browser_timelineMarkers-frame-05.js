/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function forceSyncReflow(div) {
  div.setAttribute('class', 'resize-change-color');
  // Force a reflow.
  return div.offsetWidth;
}

function testSendingEvent() {
  content.document.body.dispatchEvent(new content.Event("dog"));
}

function testConsoleTime() {
  content.console.time("cats");
}

function testConsoleTimeEnd() {
  content.console.timeEnd("cats");
}

function makePromise() {
  let resolver;
  new Promise(function(resolve, reject) {
    testConsoleTime();
    resolver = resolve;
  }).then(function(val) {
    testConsoleTimeEnd();
  });
  return resolver;
}

function resolvePromise(resolver) {
  resolver(23);
}

let TESTS = [{
  desc: "Stack trace on sync reflow",
  searchFor: "Reflow",
  setup: function(docShell) {
    let div = content.document.querySelector("div");
    forceSyncReflow(div);
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == "Reflow");
    ok(markers.length > 0, "Reflow marker includes stack");
    ok(markers[0].stack.functionDisplayName == "forceSyncReflow");
  }
}, {
  desc: "Stack trace on DOM event",
  searchFor: "DOMEvent",
  setup: function(docShell) {
    content.document.body.addEventListener("dog",
                                           function(e) { console.log("hi"); },
                                           true);
    testSendingEvent();
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == "DOMEvent");
    ok(markers.length > 0, "DOMEvent marker includes stack");
    ok(markers[0].stack.functionDisplayName == "testSendingEvent",
       "testSendingEvent is on the stack");
  }
}, {
  desc: "Stack trace on console event",
  searchFor: "ConsoleTime",
  setup: function(docShell) {
    testConsoleTime();
    testConsoleTimeEnd();
  },
  check: function(markers) {
    markers = markers.filter(m => m.name == "ConsoleTime");
    ok(markers.length > 0, "ConsoleTime marker includes stack");
    ok(markers[0].stack.functionDisplayName == "testConsoleTime",
       "testConsoleTime is on the stack");
    ok(markers[0].endStack.functionDisplayName == "testConsoleTimeEnd",
       "testConsoleTimeEnd is on the stack");
  }
}];

if (Services.prefs.getBoolPref("javascript.options.asyncstack")) {
  TESTS.push({
    desc: "Async stack trace on Promise",
    searchFor: "ConsoleTime",
    setup: function(docShell) {
      let resolver = makePromise();
      resolvePromise(resolver);
    },
    check: function(markers) {
      markers = markers.filter(m => m.name == "ConsoleTime");
      ok(markers.length > 0, "Promise marker includes stack");

      let frame = markers[0].endStack;
      ok(frame.parent.asyncParent !== null, "Parent frame has async parent");
      is(frame.parent.asyncParent.asyncCause, "Promise",
         "Async parent has correct cause");
      is(frame.parent.asyncParent.functionDisplayName, "makePromise",
         "Async parent has correct function name");
    }
  });
}

timelineContentTest(TESTS);

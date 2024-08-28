// This file expects websocket_helpers.js in the same scope.
/* import-globals-from websocket_helpers.js */

function feedback() {
  postMessage({
    type: "feedback",
    msg:
      "executing test: " +
      (current_test + 1) +
      " of " +
      tests.length +
      " tests.",
  });
}

function ok(status, msg) {
  postMessage({ type: "status", status: !!status, msg });
}

function is(a, b, msg) {
  ok(a === b, msg);
}

function isnot(a, b, msg) {
  ok(a != b, msg);
}

function finish() {
  postMessage({ type: "finish" });
}

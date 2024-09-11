/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


load(libdir + "non262.js");

if (typeof assertWarning === 'undefined') {
    var assertWarning = function assertWarning(f, pattern) {
        enableLastWarning();

        // Verify that a warning is issued.
        clearLastWarning();
        f();
        var warning = getLastWarning();
        clearLastWarning();

        disableLastWarning();

        if (warning) {
            if (!warning.message.match(pattern)) {
                throw new Error(`assertWarning failed: "${warning.message}" does not match "${pattern}"`);
            }
            return;
        }

        throw new Error("assertWarning failed: no warning");
    };
}

if (typeof assertNoWarning === 'undefined') {
    var assertNoWarning = function assertNoWarning(f, msg) {
        enableLastWarning();

        // Verify that no warning is issued.
        clearLastWarning();
        f();
        var warning = getLastWarning();
        clearLastWarning();

        disableLastWarning();

        if (warning) {
            if (msg) {
                print("assertNoWarning: " + msg);
            }

            throw Error("assertNoWarning: Unexpected warning when calling: " + f);
        }
    };
}

if (typeof assertErrorMessage === 'undefined') {
    var assertErrorMessage = function assertErrorMessage(f, ctor, test, message) {
        try {
            f();
        } catch (e) {
            // Propagate non-specific OOM errors, we never test for these with
            // assertErrorMessage, as there is no meaningful ctor.
            if (e === "out of memory")
                throw e;
            if (!(e instanceof ctor))
                throw new Error("Assertion failed: expected exception " + ctor.name + ", got " + e + (message ? `: ${message}` : ""));
            if (typeof test == "string") {
                if (test != e.message)
                    throw new Error("Assertion failed: expected " + test + ", got " + e.message + (message ? `: ${message}` : ""));
            } else {
                if (!test.test(e.message))
                    throw new Error("Assertion failed: expected " + test.toString() + ", got " + e.message + (message ? `: ${message}` : ""));
            }
            return;
        }
        throw new Error("Assertion failed: expected exception " + ctor.name + ", no exception thrown" + (message ? `: ${message}` : ""));
    };
}

if (typeof assertTypeErrorMessage === 'undefined') {
    var assertTypeErrorMessage = function assertTypeErrorMessage(f, test) {
      assertErrorMessage(f, TypeError, test);
    };
}

if (typeof assertRangeErrorMessage === 'undefined') {
    var assertRangeErrorMessage = function assertRangeErrorMessage(f, test) {
      assertErrorMessage(f, RangeError, test);
    };
}

if (typeof assertArrayEq === 'undefined') {
  var assertArrayEq = function assertArrayEq(a,b) {
    assertEq(a.length, b.length);
    for (var i = 0; i < a.length; i++) {
      assertEq(a[i], b[i]);
    }
  };
}

if (typeof assertSuppressionChain === 'undefined' && typeof globalThis.SuppressedError !== 'undefined') {

  function errorChainVerificationHelper(err, suppressions, verifier) {
    let i = 0;
    while (err instanceof SuppressedError) {
      assertEq(verifier(err.error, suppressions[i]), true);
      err = err.suppressed;
      i++;
    }
    assertEq(verifier(err, suppressions[i]), true);
    assertEq(i, suppressions.length - 1);
  }

  var assertSuppressionChain = function assertSuppressionChain(fn, suppressions) {
    let caught = false;
    try {
      fn();
    } catch (err) {
      caught = true;
      errorChainVerificationHelper(err, suppressions, function(err, suppression) {
        return err === suppression;
      });
    } finally {
      assertEq(caught, true);
    }
  }

  var assertSuppressionChainAsync = function assertSuppressionChainAsync(f, suppressions) {
    let thenCalled = false;
    let catchCalled = false;
    let e = null;
    f().then(() => { thenCalled = true; }, err => { catchCalled = true; e = err; });
    drainJobQueue();
    assertEq(thenCalled, false);
    assertEq(catchCalled, true);
    assertSuppressionChain(() => { throw e; }, suppressions);
  };

  var assertSuppressionChainErrorMessages = function assertSuppressionChainErrorMessages(fn, suppressions) {
    let caught = false;
    try {
      fn();
    } catch (err) {
      caught = true;
      errorChainVerificationHelper(err, suppressions, function(err, suppression) {
        return err instanceof suppression.ctor && err.message === suppression.message;
      });
    } finally {
      assertEq(caught, true);
    }
  }
}

if (typeof assertThrowsInstanceOfAsync === 'undefined') {
  var assertThrowsInstanceOfAsync = function assertThrowsInstanceOfAsync(f, ctor, message) {
    let thenCalled = false;
    let catchCalled = false;
    let e = null;
    f().then(() => { thenCalled = true; }, err => { catchCalled = true; e = err; });
    drainJobQueue();
    assertEq(thenCalled, false);
    assertEq(catchCalled, true);
    assertThrowsInstanceOf(() => { throw e; }, ctor, message);
  };
}

/* globals chromeWindow */

/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

// The main test function.
var test = function (isContent) {
  SimpleTest.waitForExplicitFinish();

  SpecialPowers.pushPrefEnv({
    set: [["security.allow_eval_with_system_principal", true]],
  });

  if (!isContent) {
    let { ww } = SpecialPowers.Services;
    window.chromeWindow = ww.activeWindow;
  }

  // checkPair: tests if members of pair [a, b] are equal when evaluated.
  let checkPair = function (a, b) {
    // eslint-disable-next-line no-eval
    is(eval(a), eval(b), a + " should be equal to " + b);
  };

  // Returns generator object that iterates through pref values.
  let prefVals = (function* () {
    yield false;
    yield true;
  })();

  // The main test function, runs until all pref values are exhausted.
  let nextTest = function () {
    let { value: prefValue, done } = prefVals.next();
    if (done) {
      SimpleTest.finish();
      return;
    }
    SpecialPowers.pushPrefEnv(
      { set: [["privacy.resistFingerprinting", prefValue]] },
      function () {
        let sizes = [
          [1920, 1080],
          [3840, 2160],
          [7680, 4320],
        ];
        let screenSize;
        for (let s of sizes) {
          // Make sure we always have a value, even though we do not fit.
          screenSize = s;
          if (window.outerWidth <= s[0] && window.outerHeight <= s[1]) {
            break;
          }
        }

        // These values are hardcoded in nsRFPService.cpp.
        let availOffset = 0;
        if (SpecialPowers.Services.appinfo.OS == "WINNT") {
          availOffset = 48;
        } else if (SpecialPowers.Services.appinfo.OS == "Darwin") {
          availOffset = 76;
        }

        // The pairs of values expected to be the same when
        // fingerprinting resistance is enabled.
        let pairs = [
          ["screenX", 0],
          ["screenY", 0],
          ["mozInnerScreenX", 0],
          ["mozInnerScreenY", 0],
          ["screen.pixelDepth", 24],
          ["screen.colorDepth", 24],
          ["screen.availWidth", screenSize[0]],
          ["screen.availHeight", screenSize[1] - availOffset],
          ["screen.left", 0],
          ["screen.top", 0],
          ["screen.availLeft", 0],
          ["screen.availTop", 0],
          ["screen.width", screenSize[0]],
          ["screen.height", screenSize[1]],
          ["devicePixelRatio", 2],
        ];

        // We will be resisting fingerprinting if the pref is enabled,
        // and we are in a content script (not chrome).
        let resisting = prefValue && isContent;
        // Check each of the pairs.
        pairs.map(function ([item, onVal]) {
          if (resisting) {
            checkPair("window." + item, onVal);
          } else if (!isContent && !item.startsWith("moz")) {
            checkPair("window." + item, "chromeWindow." + item);
          }
        });
        if (!isContent && !resisting) {
          // Hard to predict these values, but we can enforce constraints:
          ok(
            window.mozInnerScreenX >= chromeWindow.mozInnerScreenX,
            "mozInnerScreenX"
          );
          ok(
            window.mozInnerScreenY >= chromeWindow.mozInnerScreenY,
            "mozInnerScreenY"
          );
        }
        nextTest();
      }
    );
  };

  nextTest();
};

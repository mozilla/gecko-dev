/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator;
var testResult;

loadScript("dom/quota/test/common/nestedtest.js");

function loadScript(path) {
  const url = new URL(path, "chrome://mochitests/content/browser/");
  SpecialPowers.Services.scriptloader.loadSubScript(url.href, this);
}

function runTest() {
  clearAllDatabases(() => {
    testGenerator = testSteps();
    testGenerator.next();
  });
}

function finishTestNow() {
  if (testGenerator) {
    testGenerator.return();
    testGenerator = undefined;
  }
}

function finishTest() {
  clearAllDatabases(() => {
    setTimeout(finishTestNow, 0);
    setTimeout(() => {
      window.parent.postMessage(testResult, "*");
    }, 0);
  });
}

function continueToNextStep() {
  setTimeout(() => {
    testGenerator.next();
  }, 0);
}

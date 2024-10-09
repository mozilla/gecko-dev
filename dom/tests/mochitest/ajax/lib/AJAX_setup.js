var AJAXtests = [];

function runAJAXTest() {
  if (AJAXtests.length == 0) {
    SimpleTest.finish();
    return;
  }

  var test = AJAXtests.shift();
  var testframe = document.getElementById("testframe");
  testframe.src = test;
}

function onManifestLoad(manifest) {
  if (manifest?.testcases) {
    AJAXtests = manifest.testcases;
    runAJAXTest();
  } else {
    ok(false, "manifest check: no manifest!?!");
    SimpleTest.finish();
  }
}

function fetchManifest() {
  var d = fetch("manifest.json");
  d.then(response=>response.json()).then(onManifestLoad).catch(onManifestLoad);
}

// Double timeout duration. Since this test case takes longer than 300 seconds
// on B2G emulator.
// See bug 968783.
SimpleTest.requestLongerTimeout(2);

SimpleTest.waitForExplicitFinish();
addLoadEvent(fetchManifest);

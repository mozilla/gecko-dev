/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

TestRunner.logEnabled = true;
TestRunner.logger = LogController;

/* Helper function */
parseQueryString = function(encodedString, useArrays) {
  // strip a leading '?' from the encoded string
  var qstr = (encodedString[0] == "?") ? encodedString.substring(1) : 
                                         encodedString;
  var pairs = qstr.replace(/\+/g, "%20").split(/(\&amp\;|\&\#38\;|\&#x26;|\&)/);
  var o = {};
  var decode;
  if (typeof(decodeURIComponent) != "undefined") {
    decode = decodeURIComponent;
  } else {
    decode = unescape;
  }
  if (useArrays) {
    for (var i = 0; i < pairs.length; i++) {
      var pair = pairs[i].split("=");
      if (pair.length !== 2) {
        continue;
      }
      var name = decode(pair[0]);
      var arr = o[name];
      if (!(arr instanceof Array)) {
        arr = [];
        o[name] = arr;
      }
      arr.push(decode(pair[1]));
    }
  } else {
    for (i = 0; i < pairs.length; i++) {
      pair = pairs[i].split("=");
      if (pair.length !== 2) {
        continue;
      }
      o[decode(pair[0])] = decode(pair[1]);
    }
  }
  return o;
};

// Check the query string for arguments
var params = parseQueryString(location.search.substring(1), true);

var config = {};
if (window.readConfig) {
  config = readConfig();
}

if (config.testRoot == "chrome" || config.testRoot == "a11y") {
  for (p in params) {
    if (params[p] == 1) {
      config[p] = true;
    } else if (params[p] == 0) {
      config[p] = false;
    } else {
      config[p] = params[p];
    }
  }
  params = config;
  params.baseurl = "chrome://mochitests/content";
} else {
  params.baseurl = "";
}

if (params.testRoot == "browser") {
  params.testPrefix = "chrome://mochitests/content/browser/";
} else if (params.testRoot == "chrome") {
  params.testPrefix = "chrome://mochitests/content/chrome/";
} else if (params.testRoot == "a11y") {
  params.testPrefix = "chrome://mochitests/content/a11y/";
} else {
  params.testPrefix = "/tests/";
}

// set the per-test timeout if specified in the query string
if (params.timeout) {
  TestRunner.timeout = parseInt(params.timeout) * 1000;
}

// log levels for console and logfile
var fileLevel =  params.fileLevel || null;
var consoleLevel = params.consoleLevel || null;

// repeat tells us how many times to repeat the tests
if (params.repeat) {
  TestRunner.repeat = params.repeat;
} 

if (params.runUntilFailure) {
  TestRunner.runUntilFailure = true;
}

// closeWhenDone tells us to close the browser when complete
if (params.closeWhenDone) {
  TestRunner.onComplete = SpecialPowers.quit;
}

if (params.failureFile) {
  TestRunner.setFailureFile(params.failureFile);
}

// Breaks execution and enters the JS debugger on a test failure
if (params.debugOnFailure) {
  TestRunner.debugOnFailure = true;
}

// logFile to write our results
if (params.logFile) {
  var spl = new SpecialPowersLogger(params.logFile);
  TestRunner.logger.addListener("mozLogger", fileLevel + "", spl.getLogCallback());
}

// A temporary hack for android 4.0 where Fennec utilizes the pandaboard so much it reboots
if (params.runSlower) {
  TestRunner.runSlower = true;
}

if (params.dumpOutputDirectory) {
  TestRunner.dumpOutputDirectory = params.dumpOutputDirectory;
}

if (params.dumpAboutMemoryAfterTest) {
  TestRunner.dumpAboutMemoryAfterTest = true;
}

if (params.dumpDMDAfterTest) {
  TestRunner.dumpDMDAfterTest = true;
}

if (params.quiet) {
  TestRunner.quiet = true;
}

// Log things to the console if appropriate.
TestRunner.logger.addListener("dumpListener", consoleLevel + "", function(msg) {
  dump(msg.num + " " + msg.level + " " + msg.info.join(' ') + "\n");
});

var gTestList = [];
var RunSet = {}
RunSet.runall = function(e) {
  // Filter tests to include|exclude tests based on data in params.filter.
  // This allows for including or excluding tests from the gTestList
  if (params.testManifest) {
    getTestManifest("http://mochi.test:8888/" + params.testManifest, params, function(filter) { gTestList = filterTests(filter, gTestList, params.runOnly); RunSet.runtests(); });
  } else {
    RunSet.runtests();
  }
}

RunSet.runtests = function(e) {
  // Which tests we're going to run
  var my_tests = gTestList;

  if (params.startAt || params.endAt) {
    my_tests = skipTests(my_tests, params.startAt, params.endAt);
  }

  if (params.totalChunks && params.thisChunk) {
    my_tests = chunkifyTests(my_tests, params.totalChunks, params.thisChunk, params.chunkByDir, TestRunner.logger);
  }

  if (params.shuffle) {
    for (var i = my_tests.length-1; i > 0; --i) {
      var j = Math.floor(Math.random() * i);
      var tmp = my_tests[j];
      my_tests[j] = my_tests[i];
      my_tests[i] = tmp;
    }
  }
  TestRunner.runTests(my_tests);
}

RunSet.reloadAndRunAll = function(e) {
  e.preventDefault();
  //window.location.hash = "";
  var addParam = "";
  if (params.autorun) {
    window.location.search += "";
    window.location.href = window.location.href;
  } else if (window.location.search) {
    window.location.href += "&autorun=1";
  } else {
    window.location.href += "?autorun=1";
  }  
};

// UI Stuff
function toggleVisible(elem) {
    toggleElementClass("invisible", elem);
}

function makeVisible(elem) {
    removeElementClass(elem, "invisible");
}

function makeInvisible(elem) {
    addElementClass(elem, "invisible");
}

function isVisible(elem) {
    // you may also want to check for
    // getElement(elem).style.display == "none"
    return !hasElementClass(elem, "invisible");
};

function toggleNonTests (e) {
  e.preventDefault();
  var elems = document.getElementsByClassName("non-test");
  for (var i="0"; i<elems.length; i++) {
    toggleVisible(elems[i]);
  }
  if (isVisible(elems[0])) {
    $("toggleNonTests").innerHTML = "Hide Non-Tests";
  } else {
    $("toggleNonTests").innerHTML = "Show Non-Tests";
  }
}

// hook up our buttons
function hookup() {
  if (params.manifestFile) {
    getTestManifest("http://mochi.test:8888/" + params.manifestFile, params, hookupTests);
  } else {
    hookupTests(gTestList);
  }
}

function hookupTests(testList) {
  if (testList.length > 0) {
    gTestList = testList;
  } else {
    gTestList = [];
    for (var obj in testList) {
        gTestList.push(obj);
    }
  }

  document.getElementById('runtests').onclick = RunSet.reloadAndRunAll;
  document.getElementById('toggleNonTests').onclick = toggleNonTests; 
  // run automatically if autorun specified
  if (params.autorun) {
    RunSet.runall();
  }
}

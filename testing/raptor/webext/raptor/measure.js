/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// content script for use with pageload tests
var perfData = window.performance;
var gRetryCounter = 0;

// measure hero element; must exist inside test page;
// default only; this is set via control server settings json
var getHero = false;
var heroesToCapture = [];

// measure firefox time-to-first-non-blank-paint
// note: this browser pref must be enabled:
// dom.performance.time_to_non_blank_paint.enabled = True
// default only; this is set via control server settings json
var getFNBPaint = false;

// measure firefox domContentFlushed
// note: this browser pref must be enabled:
// dom.performance.time_to_dom_content_flushed.enabled = True
// default only; this is set via control server settings json
var getDCF = false;

// measure firefox TTFI
// note: this browser pref must be enabled:
// dom.performance.time_to_first_interactive.enabled = True
// default only; this is set via control server settings json
var getTTFI = false;

// measure google's first-contentful-paint
// default only; this is set via control server settings json
var getFCP = false;

// performance.timing measurement used as 'starttime'
var startMeasure = "fetchStart";

function contentHandler() {
  // retrieve test settings from local ext storage
  if (typeof(browser) !== "undefined") {
    // firefox, returns promise
    browser.storage.local.get("settings").then(function(item) {
      setup(item.settings);
    });
  } else {
    // chrome, no promise so use callback
    chrome.storage.local.get("settings", function(item) {
      setup(item.settings);
    });
  }
}

function setup(settings) {
  if (settings.type != "pageload") {
    return;
  }

  if (settings.measure == undefined) {
    console.log("abort: 'measure' key not found in test settings");
    return;
  }

  if (settings.measure.fnbpaint !== undefined) {
    getFNBPaint = settings.measure.fnbpaint;
    if (getFNBPaint) {
      console.log("will be measuring fnbpaint");
      measureFNBPaint();
    }
  }

  if (settings.measure.dcf !== undefined) {
    getDCF = settings.measure.dcf;
    if (getDCF) {
      console.log("will be measuring dcf");
      measureDCF();
    }
  }

  if (settings.measure.fcp !== undefined) {
    getFCP = settings.measure.fcp;
    if (getFCP) {
      console.log("will be measuring first-contentful-paint");
      measureFCP();
    }
  }

  if (settings.measure.hero !== undefined) {
    if (settings.measure.hero.length !== 0) {
      getHero = true;
      heroesToCapture = settings.measure.hero;
      console.log("hero elements to measure: " + heroesToCapture);
      measureHero();
    }
  }

  if (settings.measure.ttfi !== undefined) {
    getTTFI = settings.measure.ttfi;
    if (getTTFI) {
      console.log("will be measuring ttfi");
      measureTTFI();
    }
  }
}

function measureHero() {
  var obs = null;

  var heroElementsFound = window.document.querySelectorAll("[elementtiming]");
  console.log("found " + heroElementsFound.length + " hero elements in the page");

  if (heroElementsFound) {
    function callbackHero(entries, observer) {
      entries.forEach(entry => {
        var heroFound = entry.target.getAttribute("elementtiming");
        // mark the time now as when hero element received
        perfData.mark(heroFound);
        console.log("found hero:" + heroFound);
        // calculcate result: performance.timing.fetchStart - time when we got hero element
        perfData.measure(name = resultType,
                         startMark = startMeasure,
                         endMark = heroFound);
        var perfResult = perfData.getEntriesByName(resultType);
        var _result = Math.round(perfResult[0].duration);
        var resultType = "hero:" + heroFound;
        sendResult(resultType, _result);
        perfData.clearMarks();
        perfData.clearMeasures();
        obs.disconnect();
      });
    }
    // we want the element 100% visible on the viewport
    var options = {root: null, rootMargin: "0px", threshold: [1]};
    try {
      obs = new window.IntersectionObserver(callbackHero, options);
      heroElementsFound.forEach(function(el) {
        // if hero element is one we want to measure, add it to the observer
        if (heroesToCapture.indexOf(el.getAttribute("elementtiming")) > -1)
          obs.observe(el);
      });
    } catch (err) {
      console.log(err);
    }
  } else {
      console.log("couldn't find hero element");
  }

}

function measureFNBPaint() {
  var x = window.performance.timing.timeToNonBlankPaint;

  if (typeof(x) == "undefined") {
    console.log("ERROR: timeToNonBlankPaint is undefined; ensure the pref is enabled");
    return;
  }
  if (x > 0) {
    console.log("got fnbpaint");
    gRetryCounter = 0;
    var startTime = perfData.timing.fetchStart;
    sendResult("fnbpaint", x - startTime);
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      console.log("\nfnbpaint is not yet available (0), retry number " + gRetryCounter + "...\n");
      window.setTimeout(measureFNBPaint, 100);
    } else {
      console.log("\nunable to get a value for fnbpaint after " + gRetryCounter + " retries\n");
    }
  }
}

function measureDCF() {
  var x = window.performance.timing.timeToDOMContentFlushed;

  if (typeof(x) == "undefined") {
    console.log("ERROR: domContentFlushed is undefined; ensure the pref is enabled");
    return;
  }
  if (x > 0) {
    console.log("got domContentFlushed: " + x);
    gRetryCounter = 0;
    var startTime = perfData.timing.fetchStart;
    sendResult("dcf", x - startTime);
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      console.log("\dcf is not yet available (0), retry number " + gRetryCounter + "...\n");
      window.setTimeout(measureDCF, 100);
    } else {
      console.log("\nunable to get a value for dcf after " + gRetryCounter + " retries\n");
    }
  }
}

function measureTTFI() {
  var x = window.performance.timing.timeToFirstInteractive;

  if (typeof(x) == "undefined") {
    console.log("ERROR: timeToFirstInteractive is undefined; ensure the pref is enabled");
    return;
  }
  if (x > 0) {
    console.log("got timeToFirstInteractive: " + x);
    gRetryCounter = 0;
    var startTime = perfData.timing.fetchStart;
    sendResult("ttfi", x - startTime);
  } else {
    gRetryCounter += 1;
    // NOTE: currently the gecko implementation doesn't look at network
    // requests, so this is closer to TimeToFirstInteractive than
    // TimeToInteractive.  Also, we use FNBP instead of FCP as the start
    // point.  TTFI/TTI requires running at least 5 seconds past last
    // "busy" point, give 25 seconds here (overall the harness times out at
    // 30 seconds).  Some pages will never get 5 seconds without a busy
    // period!
    if (gRetryCounter <= 25 * (1000 / 200)) {
      console.log("TTFI is not yet available (0), retry number " + gRetryCounter + "...\n");
      window.setTimeout(measureTTFI, 200);
    } else {
      // unable to get a value for TTFI - negative value will be filtered out later
      console.log("TTFI was not available for this pageload");
      sendResult("ttfi", -1);
    }
  }
}

function measureFCP() {
  // see https://developer.mozilla.org/en-US/docs/Web/API/PerformancePaintTiming
  var resultType = "fcp";
  var result = 0;

  let perfEntries = perfData.getEntriesByType("paint");

  if (perfEntries.length >= 2) {
    if (perfEntries[1].name == "first-contentful-paint" && perfEntries[1].startTime != undefined)
      result = perfEntries[1].startTime;
  }

  if (result > 0) {
    console.log("got time to first-contentful-paint");
    sendResult(resultType, result);
    perfData.clearMarks();
    perfData.clearMeasures();
  } else {
    gRetryCounter += 1;
    if (gRetryCounter <= 10) {
      console.log("\ntime to first-contentful-paint is not yet available (0), retry number " + gRetryCounter + "...\n");
      window.setTimeout(measureFCP, 100);
    } else {
      console.log("\nunable to get a value for time-to-fcp after " + gRetryCounter + " retries\n");
    }
  }
}

function sendResult(_type, _value) {
  // send result back to background runner script
  console.log("sending result back to runner: " + _type + " " + _value);
  chrome.runtime.sendMessage({"type": _type, "value": _value}, function(response) {
    if (response !== undefined) {
      console.log(response.text);
    }
  });
}

window.onload = contentHandler();

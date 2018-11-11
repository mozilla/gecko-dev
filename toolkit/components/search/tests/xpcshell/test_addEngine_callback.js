/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests covering nsIBrowserSearchService::addEngine's optional callback.
 */

ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");

"use strict";

// Only need to stub the methods actually called by nsSearchService
var promptService = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPromptService]),
  confirmEx() {},
};
var prompt = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
  alert() {},
};
// Override the prompt service and nsIPrompt, since the search service currently
// prompts in response to certain installation failures we test here
// XXX this should disappear once bug 863474 is fixed
MockRegistrar.register("@mozilla.org/embedcomp/prompt-service;1", promptService);
MockRegistrar.register("@mozilla.org/prompter;1", prompt);


// First test inits the search service
add_test(function init_search_service() {
  Services.search.init(function(status) {
    if (!Components.isSuccessCode(status))
      do_throw("Failed to initialize search service");

    run_next_test();
  });
});

// Simple test of the search callback
add_test(function simple_callback_test() {
  let searchCallback = {
    onSuccess(engine) {
      Assert.ok(!!engine);
      Assert.notEqual(engine.name, Services.search.defaultEngine.name);
      Assert.equal(engine.wrappedJSObject._loadPath,
                   "[http]localhost/test-search-engine.xml");
      run_next_test();
    },
    onError(errorCode) {
      do_throw("search callback returned error: " + errorCode);
    },
  };
  Services.search.addEngine(gDataUrl + "engine.xml", null,
                            false, searchCallback);
});

// Test of the search callback on duplicate engine failures
add_test(function duplicate_failure_test() {
  let searchCallback = {
    onSuccess(engine) {
      do_throw("this addition should not have succeeded");
    },
    onError(errorCode) {
      Assert.ok(!!errorCode);
      Assert.equal(errorCode, Ci.nsISearchInstallCallback.ERROR_DUPLICATE_ENGINE);
      run_next_test();
    },
  };
  // Re-add the same engine added in the previous test
  Services.search.addEngine(gDataUrl + "engine.xml", null,
                            false, searchCallback);
});

// Test of the search callback on failure to load the engine failures
add_test(function load_failure_test() {
  let searchCallback = {
    onSuccess(engine) {
      do_throw("this addition should not have succeeded");
    },
    onError(errorCode) {
      Assert.ok(!!errorCode);
      Assert.equal(errorCode, Ci.nsISearchInstallCallback.ERROR_UNKNOWN_FAILURE);
      run_next_test();
    },
  };
  // Try adding an engine that doesn't exist
  Services.search.addEngine("http://invalid/data/engine.xml", null,
                            false, searchCallback);
});

function run_test() {
  useHttpServer();

  run_next_test();
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ReactDOMServer, NewtabRenderUtils */

const PAGE_TEMPLATE_RESOURCE_PATH =
  "resource://newtab/data/content/abouthomecache/page.html.template";
const SCRIPT_TEMPLATE_RESOURCE_PATH =
  "resource://newtab/data/content/abouthomecache/script.js.template";

// If we don't stub these functions out, React throws warnings in the console
// upon being loaded.
let window = self;
window.requestAnimationFrame = () => {};
window.cancelAnimationFrame = () => {};

/* import-globals-from /toolkit/components/workerloader/require.js */
importScripts("resource://gre/modules/workers/require.js");

{
  let oldChromeUtils = ChromeUtils;

  // ChromeUtils is defined inside of a Worker, but we don't want the
  // activity-stream.bundle.js to detect it when loading, since that results
  // in it attempting to import JSMs on load, which is not allowed in
  // a Worker. So we temporarily clear ChromeUtils so that activity-stream.bundle.js
  // thinks its being loaded in content scope.
  //
  // eslint-disable-next-line no-implicit-globals, no-global-assign
  ChromeUtils = undefined;

  /* import-globals-from ../../../../toolkit/content/vendor/react/react.js */
  /* import-globals-from ../../../../toolkit/content/vendor/react/react-dom.js */
  /* import-globals-from ../../../../toolkit/content/vendor/react/react-dom-server.js */
  /* import-globals-from ../../../../toolkit/content/vendor/react/redux.js */
  /* import-globals-from ../../../../toolkit/content/vendor/react/react-transition-group.js */
  /* import-globals-from ../../../../toolkit/content/vendor/react/prop-types.js */
  /* import-globals-from ../../../../toolkit/content/vendor/react/react-redux.js */
  /* import-globals-from ../data/content/activity-stream.bundle.js */
  importScripts(
    "chrome://global/content/vendor/react.js",
    "chrome://global/content/vendor/react-dom.js",
    "chrome://global/content/vendor/react-dom-server.js",
    "chrome://global/content/vendor/redux.js",
    "chrome://global/content/vendor/react-transition-group.js",
    "chrome://global/content/vendor/prop-types.js",
    "chrome://global/content/vendor/react-redux.js",
    "resource://newtab/data/content/activity-stream.bundle.js"
  );

  // eslint-disable-next-line no-global-assign, no-implicit-globals
  ChromeUtils = oldChromeUtils;
}

let PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

let Agent = {
  _templates: null,

  /**
   * Synchronously loads the template files off of the file
   * system, and returns them as an object. If the Worker has loaded
   * these templates before, a cached copy of the templates is returned
   * instead.
   *
   * @return Object
   *   An object with the following properties:
   *
   *   pageTemplate (String):
   *     The template for the document markup.
   *
   *   scriptTempate (String):
   *     The template for the script.
   */
  getOrCreateTemplates() {
    if (this._templates) {
      return this._templates;
    }

    const templateResources = new Map([
      ["pageTemplate", PAGE_TEMPLATE_RESOURCE_PATH],
      ["scriptTemplate", SCRIPT_TEMPLATE_RESOURCE_PATH],
    ]);

    this._templates = {};

    for (let [templateName, path] of templateResources) {
      const xhr = new XMLHttpRequest();
      // Using a synchronous XHR in a worker is fine.
      xhr.open("GET", path, false);
      xhr.responseType = "text";
      xhr.send(null);
      this._templates[templateName] = xhr.responseText;
    }

    return this._templates;
  },

  /**
   * Constructs the cached about:home document using ReactDOMServer. This will
   * be called when "construct" messages are sent to this PromiseWorker.
   *
   * @param state (Object)
   *   The most recent Activity Stream Redux state.
   * @return Object
   *   An object with the following properties:
   *
   *   page (String):
   *     The generated markup for the document.
   *
   *   script (String):
   *     The generated script for the document.
   */
  construct(state) {
    // If anything in this function throws an exception, PromiseWorker
    // runs the risk of leaving the Promise associated with this method
    // forever unresolved. This is particularly bad when this method is
    // called via AsyncShutdown, since the forever unresolved Promise can
    // result in a AsyncShutdown timeout crash.
    //
    // To help ensure that no matter what, the Promise resolves with something,
    // we wrap the whole operation in a try/catch.
    try {
      return this._construct(state);
    } catch (e) {
      console.error("about:home startup cache construction failed:", e);
      return { page: null, script: null };
    }
  },

  /**
   * Internal method that actually does the work of constructing the cached
   * about:home document using ReactDOMServer. This should be called from
   * `construct` only.
   *
   * @param state (Object)
   *   The most recent Activity Stream Redux state.
   * @return Object
   *   An object with the following properties:
   *
   *   page (String):
   *     The generated markup for the document.
   *
   *   script (String):
   *     The generated script for the document.
   */
  _construct(state) {
    for (const key of Object.keys(state.App.isForStartupCache)) {
      state.App.isForStartupCache[key] = true;
    }

    // ReactDOMServer.renderToString expects a Redux store to pull
    // the state from, so we mock out a minimal store implementation.
    let fakeStore = {
      getState() {
        return state;
      },
      dispatch() {},
    };

    let markup = ReactDOMServer.renderToString(
      NewtabRenderUtils.NewTab({
        store: fakeStore,
        isFirstrun: false,
      })
    );

    let { pageTemplate, scriptTemplate } = this.getOrCreateTemplates();
    let cacheTime = new Date().toUTCString();
    let page = pageTemplate
      .replace("{{ MARKUP }}", markup)
      .replace("{{ CACHE_TIME }}", cacheTime);
    let script = scriptTemplate.replace(
      "{{ STATE }}",
      JSON.stringify(state, null, "\t")
    );

    return { page, script };
  },
};

// This boilerplate connects the PromiseWorker to the Agent so
// that messages from the main thread map to methods on the
// Agent.
let worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function (method, args = []) {
  return Agent[method](...args);
};
worker.postMessage = function (result, ...transfers) {
  self.postMessage(result, ...transfers);
};
worker.close = function () {
  self.close();
};

self.addEventListener("message", msg => worker.handleMessage(msg));
self.addEventListener("unhandledrejection", function (error) {
  throw error.reason;
});

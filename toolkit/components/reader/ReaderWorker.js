/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A worker dedicated to handle parsing documents for reader view.
 */

importScripts("resource://gre/modules/workers/require.js",
              "resource://gre/modules/reader/JSDOMParser.js",
              "resource://gre/modules/reader/Readability.js");

let PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

const DEBUG = false;

let worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function(method, args = []) {
  return Agent[method](...args);
};
worker.postMessage = function(result, ...transfers) {
  self.postMessage(result, ...transfers);
};
worker.close = function() {
  self.close();
};
worker.log = function(...args) {
  if (DEBUG) {
    dump("ReaderWorker: " + args.join(" ") + "\n");
  }
};

self.addEventListener("message", msg => worker.handleMessage(msg));

let Agent = {
  /**
   * Parses structured article data from a document.
   *
   * @param {object} uri URI data for the document.
   * @param {string} serializedDoc The serialized document.
   *
   * @return {object} Article object returned from Readability.
   */
  parseDocument: function (uri, serializedDoc) {
    let doc = new JSDOMParser().parse(serializedDoc);
    return new Readability(uri, doc).parse();
  },
};

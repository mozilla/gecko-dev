/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* global addMessageListener, addEventListener, content, sendAsyncMessage */

/**
 * This frame script injects itself into perf-html.io and injects the profile
 * into the page. It is mostly taken from the Gecko Profiler Addon implementation.
 */

const TRANSFER_EVENT = "devtools:perf-html-transfer-profile";
const SYMBOL_TABLE_REQUEST_EVENT = "devtools:perf-html-request-symbol-table";
const SYMBOL_TABLE_RESPONSE_EVENT = "devtools:perf-html-reply-symbol-table";

let gProfile = null;
const symbolReplyPromiseMap = new Map();

addMessageListener(TRANSFER_EVENT, e => {
  gProfile = e.data;
  // Eagerly try and see if the framescript was evaluated after perf loaded its scripts.
  connectToPage();
  // If not try again at DOMContentLoaded which should be called after the script
  // tag was synchronously loaded in.
  addEventListener("DOMContentLoaded", connectToPage);
});

addMessageListener(SYMBOL_TABLE_RESPONSE_EVENT, e => {
  const { debugName, breakpadId, status, result, error } = e.data;
  const promiseKey = [debugName, breakpadId].join(":");
  const { resolve, reject } = symbolReplyPromiseMap.get(promiseKey);
  symbolReplyPromiseMap.delete(promiseKey);

  if (status === "success") {
    const [addresses, index, buffer] = result;
    resolve([addresses, index, buffer]);
  } else {
    reject(error);
  }
});

function connectToPage() {
  const unsafeWindow = content.wrappedJSObject;
  if (unsafeWindow.connectToGeckoProfiler) {
    unsafeWindow.connectToGeckoProfiler(makeAccessibleToPage({
      getProfile: () => Promise.resolve(gProfile),
      getSymbolTable: (debugName, breakpadId) => getSymbolTable(debugName, breakpadId),
    }, unsafeWindow));
  }
}

function getSymbolTable(debugName, breakpadId) {
  return new Promise((resolve, reject) => {
    sendAsyncMessage(SYMBOL_TABLE_REQUEST_EVENT, { debugName, breakpadId });
    symbolReplyPromiseMap.set([debugName, breakpadId].join(":"), {
      resolve,
      reject,
    });
  });
}

// The following functions handle the security of cloning the object into the page.
// The code was taken from the original Gecko Profiler Add-on to maintain
// compatibility with the existing profile importing mechanism:
// See: https://github.com/devtools-html/Gecko-Profiler-Addon/blob/78138190b42565f54ce4022a5b28583406489ed2/data/tab-framescript.js

/**
 * Create a promise that can be used in the page.
 */
function createPromiseInPage(fun, contentGlobal) {
  function funThatClonesObjects(resolve, reject) {
    return fun(result => resolve(Cu.cloneInto(result, contentGlobal)),
               error => reject(Cu.cloneInto(error, contentGlobal)));
  }
  return new contentGlobal.Promise(Cu.exportFunction(funThatClonesObjects,
                                                     contentGlobal));
}

/**
 * Returns a function that calls the original function and tries to make the
 * return value available to the page.
 */
function wrapFunction(fun, contentGlobal) {
  return function() {
    const result = fun.apply(this, arguments);
    if (typeof result === "object") {
      if (("then" in result) && (typeof result.then === "function")) {
        // fun returned a promise.
        return createPromiseInPage((resolve, reject) =>
          result.then(resolve, reject), contentGlobal);
      }
      return Cu.cloneInto(result, contentGlobal);
    }
    return result;
  };
}

/**
 * Pass a simple object containing values that are objects or functions.
 * The objects or functions are wrapped in such a way that they can be
 * consumed by the page.
 */
function makeAccessibleToPage(obj, contentGlobal) {
  const result = Cu.createObjectIn(contentGlobal);
  for (const field in obj) {
    switch (typeof obj[field]) {
      case "function":
        Cu.exportFunction(
          wrapFunction(obj[field], contentGlobal), result, { defineAs: field });
        break;
      case "object":
        Cu.cloneInto(obj[field], result, { defineAs: field });
        break;
      default:
        result[field] = obj[field];
        break;
    }
  }
  return result;
}

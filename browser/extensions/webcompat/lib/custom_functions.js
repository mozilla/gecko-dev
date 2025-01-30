/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser, module */

const replaceStringInRequest = (
  requestId,
  inString,
  outString,
  inEncoding = "utf-8"
) => {
  const filter = browser.webRequest.filterResponseData(requestId);
  const decoder = new TextDecoder(inEncoding);
  const encoder = new TextEncoder();
  const RE = new RegExp(inString, "g");
  const carryoverLength = inString.length;
  let carryover = "";

  filter.ondata = event => {
    const replaced = (
      carryover + decoder.decode(event.data, { stream: true })
    ).replace(RE, outString);
    filter.write(encoder.encode(replaced.slice(0, -carryoverLength)));
    carryover = replaced.slice(-carryoverLength);
  };

  filter.onstop = () => {
    if (carryover.length) {
      filter.write(encoder.encode(carryover));
    }
    filter.close();
  };
};

const CUSTOM_FUNCTIONS = {
  replace_string_in_request: {
    details: ["find", "replace", "urls", "types"],
    enable(details) {
      const { find, replace, urls, types } = details;
      const listener = (details.listener = ({ requestId }) => {
        replaceStringInRequest(requestId, find, replace);
        return {};
      });
      browser.webRequest.onBeforeRequest.addListener(
        listener,
        { urls, types },
        ["blocking"]
      );
    },
    disable(details) {
      const { listener } = details;
      browser.webRequest.onBeforeRequest.removeListener(listener);
      delete details.listener;
    },
  },
  run_script_before_request: {
    details: ["message", "urls", "script", "types"],
    enable(details, intervention) {
      const { bug } = intervention;
      const { message, script, types, urls } = details;
      const warning = `${message} See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`;

      const listener = (details.listener = evt => {
        const { tabId, frameId } = evt;
        return browser.tabs
          .executeScript(tabId, {
            file: script,
            frameId,
            runAt: "document_start",
          })
          .then(() => {
            browser.tabs.executeScript(tabId, {
              code: `console.warn(${JSON.stringify(warning)})`,
              runAt: "document_start",
            });
          })
          .catch(err => {
            console.error(
              "Error running script before request for webcompat intervention for bug",
              bug,
              err
            );
          });
      });

      browser.webRequest.onBeforeRequest.addListener(
        listener,
        { urls, types: types || ["script"] },
        ["blocking"]
      );
    },
    disable(details) {
      const { listener } = details;
      browser.webRequest.onBeforeRequest.removeListener(listener);
      delete details.listener;
    },
  },
};

module.exports = CUSTOM_FUNCTIONS;

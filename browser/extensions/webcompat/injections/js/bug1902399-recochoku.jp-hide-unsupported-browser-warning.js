/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1902399 - hide browser warning on recochoku.jp
 */

const callback = (mutations, observer) => {
  console.error(mutations, observer);
  const search = document.evaluate(
    "//*[text()[contains(., 'Chromeブラウザの最新版をご利用ください')]]",
    document,
    null,
    4
  );
  const found = search.iterateNext();
  if (found) {
    found.closest(".header-caution").remove();
    observer?.disconnect();
  }
};

const observer = new MutationObserver(callback);
observer.observe(document.documentElement, {
  childList: true,
  subtree: true,
});

window.addEventListener("DOMContentLoaded", () => {
  const mutations = observer.takeRecords();
  observer.disconnect();
  if (mutations.length) {
    callback(mutations);
  }
});

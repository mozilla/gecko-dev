/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* globals browser, embedHelperLib */

if (!window.smartblockDisqusShimInitialized) {
  // Guard against this script running multiple times
  window.smartblockDisqusShimInitialized = true;

  // Get the script URL from the page. We can't hardcode it because the
  // subdomain is site specific.
  let scriptURL = document.querySelector(
    'script[src*=".disqus.com/embed.js"]'
  )?.src;
  if (scriptURL) {
    embedHelperLib.initEmbedShim({
      shimId: "DisqusEmbed",
      scriptURL,
      embedLogoURL: "https://smartblock.firefox.etp/disqus.svg",
      embedSelector: "#disqus_thread",
      isTestShim: false,
    });
  }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_DOMAIN = "https://example.com/";
const TEST_PATH = "browser/toolkit/components/url-classifier/tests/browser/";
const TEST_PAGE = TEST_DOMAIN + TEST_PATH + "page.html";

async function loadImage(browser, url) {
  return SpecialPowers.spawn(browser, [url], page => {
    return new Promise(resolve => {
      let image = new content.Image();
      image.src = page + "?" + Math.random();
      image.onload = _ => resolve(true);
      image.onerror = _ => resolve(false);
    });
  });
}

function checkChannelClassificationsFlags(expectedURLPrePath, flags) {
  return TestUtils.topicObserved("http-on-modify-request", subject => {
    let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!httpChannel.URI.spec.startsWith(expectedURLPrePath)) {
      // this is not the request we are looking for
      // we use the prepath and not the spec because the query is randomly generated in the
      // loaded image
      return false;
    }

    ok(
      subject.QueryInterface(Ci.nsIClassifiedChannel).classificationFlags &
        flags,
      "Classification flags should match expected"
    );

    return true;
  });
}

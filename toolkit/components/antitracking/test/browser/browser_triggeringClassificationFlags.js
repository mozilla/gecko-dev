/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_CASES = [
  {
    name: "XHR",
    url: TEST_4TH_PARTY_PAGE_HTTPS,
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        let xhr = new content.XMLHttpRequest();
        xhr.open("GET", srcUrl);
        xhr.send();
      });
    },
  },
  {
    name: "Fetch",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "fetch.html",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        await content.fetch(srcUrl);
      });
    },
  },
  {
    name: "Image",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "raptor.jpg",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        await new content.Promise(resolve => {
          let img = content.document.createElement("img");
          img.src = srcUrl;
          img.onload = resolve;
          content.document.body.appendChild(img);
        });
      });
    },
  },
  {
    name: "CSS",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "style.css",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        await new content.Promise(resolve => {
          let link = content.document.createElement("link");
          link.rel = "stylesheet";
          link.href = srcUrl;
          link.onload = resolve;
          content.document.head.appendChild(link);
        });
      });
    },
  },
  {
    name: "Video",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "short.mp4",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        await new content.Promise(resolve => {
          let video = content.document.createElement("video");
          video.src = srcUrl;
          video.onloadeddata = resolve;
          content.document.body.appendChild(video);
        });
      });
    },
  },
  {
    name: "Audio",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "empty_size.mp3",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        await new content.Promise(resolve => {
          let audio = content.document.createElement("audio");
          audio.src = srcUrl;
          audio.onloadeddata = resolve;
          content.document.body.appendChild(audio);
        });
      });
    },
  },
  {
    name: "Iframe",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "page.html",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        await new content.Promise(resolve => {
          let iframe = content.document.createElement("iframe");
          iframe.src = srcUrl;
          iframe.onload = resolve;
          content.document.body.appendChild(iframe);
        });
      });
    },
  },
  {
    name: "Script",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "empty.js",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], async srcUrl => {
        await new content.Promise(resolve => {
          let script = content.document.createElement("script");
          script.src = srcUrl;
          script.onload = resolve;
          content.document.body.appendChild(script);
        });
      });
    },
  },
  {
    name: "Font",
    url: TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "test.font.woff",
    testFunc: async (browsingContext, url) => {
      await SpecialPowers.spawn(browsingContext, [url], srcUrl => {
        let fontName = "TestFont" + Math.random().toString(36).substring(2);
        let style = content.document.createElement("style");
        style.textContent = `@font-face { font-family: ${fontName}; src: url('${srcUrl}'); }`;
        content.document.head.appendChild(style);
        let span = content.document.createElement("span");
        span.textContent = "FontTest";
        span.style.fontFamily = fontName;
        content.document.body.appendChild(span);
        // Force layout to trigger font load
        content.getComputedStyle(span).fontFamily;
      });
    },
  },
  // The url of a websocket channel is still using http scheme in the observer.
  // So we need to use a http url but not a ws url. But the actual websocket
  // request is still made to the ws url.
  {
    name: "WebSocket",
    url: "http://mochi.test:8888/browser/toolkit/components/antitracking/test/browser/file_ws_handshake_delay",
    testFunc: async (browsingContext, _) => {
      await SpecialPowers.spawn(browsingContext, [], async _ => {
        let ws = new content.WebSocket(
          "ws://mochi.test:8888/browser/toolkit/components/antitracking/test/browser/file_ws_handshake_delay",
          ["test"]
        );

        await ContentTaskUtils.waitForEvent(ws, "open");
      });
    },
  },
];

// A helper function to observe and check flags on the channel.
function observeAndCheck(
  url,
  expectedFirstPartyFlags,
  expectedThirdPartyFlags
) {
  return new Promise(resolve => {
    let observer = {
      observe(subject, topic) {
        if (topic !== "http-on-opening-request") {
          return;
        }
        let channel = subject.QueryInterface(Ci.nsIHttpChannel);
        if (!channel || channel.URI.spec !== url) {
          return;
        }

        let loadInfo = channel.loadInfo;
        is(
          loadInfo.triggeringFirstPartyClassificationFlags,
          expectedFirstPartyFlags,
          `Correct first party flags for ${url}`
        );
        is(
          loadInfo.triggeringThirdPartyClassificationFlags,
          expectedThirdPartyFlags,
          `Correct third party flags for ${url}`
        );

        Services.obs.removeObserver(observer, "http-on-opening-request");
        resolve();
      },
    };
    Services.obs.addObserver(observer, "http-on-opening-request");
  });
}

add_setup(async function () {
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(async _ => {
    UrlClassifierTestUtils.cleanupTestTrackers();

    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
        resolve()
      );
    });
  });
});

// Test that the third party flags are set correctly when the request is made
// from a third party iframe.
add_task(async function test_third_party_flags() {
  // Open the test page and inject the tracking iframe
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  // Creating a tracking iframe.
  let trackingIframeBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_3RD_PARTY_PAGE_HTTP],
    async iframeUrl => {
      let iframe = content.document.createElement("iframe");
      iframe.src = iframeUrl;
      content.document.body.appendChild(iframe);

      await new content.Promise(resolve => {
        iframe.onload = resolve;
      });

      return iframe.browsingContext;
    }
  );

  for (const testCase of TEST_CASES) {
    info(`Running classification flags test for: ${testCase.name}`);

    let obsPromise = observeAndCheck(
      testCase.url,
      0,
      Ci.nsIClassifiedChannel.CLASSIFIED_TRACKING
    );

    // Call the testing function to make the request.
    await testCase.testFunc(trackingIframeBC, testCase.url);

    await obsPromise;
  }

  BrowserTestUtils.removeTab(tab);
});

// Test that the first party flags are set correctly when the request is made
// from an about:blank iframe in a tracking iframe.
add_task(async function test_third_party_flags_in_about_blank_iframe() {
  // Open the test page and inject the tracking iframe
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  // Creating a tracking iframe.
  let trackingIframeBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_3RD_PARTY_PAGE_HTTP],
    async iframeUrl => {
      let iframe = content.document.createElement("iframe");
      iframe.src = iframeUrl;
      content.document.body.appendChild(iframe);

      await new content.Promise(resolve => {
        iframe.onload = resolve;
      });

      return iframe.browsingContext;
    }
  );

  // Open an about:blank iframe in the tracking iframe.
  let aboutBlankIframeBC = await SpecialPowers.spawn(
    trackingIframeBC,
    [],
    async () => {
      let iframe = content.document.createElement("iframe");
      content.document.body.appendChild(iframe);

      return iframe.browsingContext;
    }
  );

  for (const testCase of TEST_CASES) {
    info(`Running classification flags test for: ${testCase.name}`);

    let obsPromise = observeAndCheck(
      testCase.url,
      0,
      Ci.nsIClassifiedChannel.CLASSIFIED_TRACKING
    );

    // Call the testing function to make the request.
    await testCase.testFunc(aboutBlankIframeBC, testCase.url);

    await obsPromise;
  }

  BrowserTestUtils.removeTab(tab);
});

// Test that the first party flags are set correctly when the request is made
// from the top-level context loading a tracking domain.
add_task(async function test_first_party_flags() {
  // Open the tab that loads a tracking domain.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_3RD_PARTY_PAGE_HTTP
  );

  for (const testCase of TEST_CASES) {
    info(`Running first-party classification flags test for: ${testCase.name}`);

    let obsPromise = observeAndCheck(
      testCase.url,
      Ci.nsIClassifiedChannel.CLASSIFIED_TRACKING,
      0
    );

    // Call the testing function to make the request.
    await testCase.testFunc(tab.linkedBrowser, testCase.url);

    await obsPromise;
  }

  BrowserTestUtils.removeTab(tab);
});

// Test that the first party flags are set correctly when the request is made
// from an about:blank iframe in the top-level context loading a tracking domain.
add_task(async function test_first_party_flags_in_about_blank_iframe() {
  // Open the tab that loads a tracking domain.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_3RD_PARTY_PAGE_HTTP
  );

  // Open an about:blank iframe in the top-level context.
  let aboutBlankIframeBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      let iframe = content.document.createElement("iframe");
      content.document.body.appendChild(iframe);
      return iframe.browsingContext;
    }
  );

  for (const testCase of TEST_CASES) {
    info(
      `Running first-party classification flags test (about:blank iframe) for: ${testCase.name}`
    );

    let obsPromise = observeAndCheck(
      testCase.url,
      Ci.nsIClassifiedChannel.CLASSIFIED_TRACKING,
      0
    );

    // Call the testing function to make the request.
    await testCase.testFunc(aboutBlankIframeBC, testCase.url);

    await obsPromise;
  }

  BrowserTestUtils.removeTab(tab);
});

// Test that loads from an embedded tracking script have the correct flags.
add_task(async function test_embedded_tracking_script() {
  // Open the test page and inject the tracking iframe
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  for (const testCase of TEST_CASES) {
    // Skip the font face test because we don't have a definite way to determine
    // who triggers the load in this case.
    if (testCase.name === "Font") {
      continue;
    }

    let obsPromise = observeAndCheck(
      testCase.url,
      0,
      Ci.nsIClassifiedChannel.CLASSIFIED_TRACKING
    );

    // Inject a tracking script into the page.
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [
        TEST_3RD_PARTY_DOMAIN_HTTP +
          TEST_PATH +
          "triggerLoads.sjs?type=" +
          testCase.name +
          "&url=" +
          testCase.url,
      ],
      async src => {
        let script = content.document.createElement("script");
        script.src = src;
        content.document.body.appendChild(script);
      }
    );

    await obsPromise;
  }

  BrowserTestUtils.removeTab(tab);
});

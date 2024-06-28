/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "https://example.com/browser/" + RELATIVE_DIR;
const pdfUrl = TESTROOT + "file_pdfjs_test.pdf";

// Test telemetry.
add_task(async function test() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  info("Pref action: " + handlerInfo.preferredAction);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function test_chrome_events(browser) {
      const promise = new Promise(resolve => {
        const removeListener = BrowserTestUtils.addContentEventListener(
          browser,
          "message",
          () => {
            info("Received 'message' event");
            removeListener();
            resolve(
              SpecialPowers.spawn(
                browser,
                [],
                async () =>
                  new Promise(res => {
                    const eventNames = [
                      "pagesloaded",
                      "layersloaded",
                      "outlineloaded",
                    ];
                    let counter = eventNames.length;
                    const callback = name => {
                      info("Received event: " + name);
                      if (--counter === 0) {
                        res();
                      }
                    };
                    for (const eventName of eventNames) {
                      info("Adding chrome listener for " + eventName);
                      docShell.chromeEventHandler.addEventListener(
                        eventName,
                        callback.bind(null, eventName),
                        {
                          once: true,
                          capture: true,
                        }
                      );
                    }
                  })
              )
            );
          },
          { capture: true },
          e =>
            ["complete", "progress", "supportsRangedLoading"].includes(
              e.data.pdfjsLoadAction
            )
        );
      });
      BrowserTestUtils.startLoadingURIString(browser, pdfUrl);
      await promise;

      await waitForPdfJSClose(browser);
    }
  );
});

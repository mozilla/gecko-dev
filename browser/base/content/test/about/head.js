/* eslint-env mozilla/frame-script */

function injectErrorPageFrame(tab, src) {
  return ContentTask.spawn(tab.linkedBrowser, {frameSrc: src}, async function({frameSrc}) {
    let loaded = ContentTaskUtils.waitForEvent(content.wrappedJSObject, "DOMFrameContentLoaded");
    let iframe = content.document.createElement("iframe");
    iframe.src = frameSrc;
    content.document.body.appendChild(iframe);
    await loaded;
    // We will have race conditions when accessing the frame content after setting a src,
    // so we can't wait for AboutNetErrorLoad. Let's wait for the certerror class to
    // appear instead (which should happen at the same time as AboutNetErrorLoad).
    await ContentTaskUtils.waitForCondition(() =>
      iframe.contentDocument.body.classList.contains("certerror"));
  });
}

async function openErrorPage(src, useFrame) {
  let dummyPage = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "dummy_page.html";

  let tab;
  if (useFrame) {
    info("Loading cert error page in an iframe");
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, dummyPage);
    await injectErrorPageFrame(tab, src);
  } else {
    let certErrorLoaded;
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, src);
      let browser = gBrowser.selectedBrowser;
      certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
    }, false);
    info("Loading and waiting for the cert error");
    await certErrorLoaded;
  }

  return tab;
}

function waitForCondition(condition, nextTest, errorMsg, retryTimes) {
  retryTimes = typeof retryTimes !== "undefined" ? retryTimes : 30;
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= retryTimes) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function whenTabLoaded(aTab, aCallback) {
  promiseTabLoadEvent(aTab).then(aCallback);
}

function promiseTabLoaded(aTab) {
  return new Promise(resolve => {
    whenTabLoaded(aTab, resolve);
  });
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url)
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);

  return loaded;
}

/**
 * Waits for the next top-level document load in the current browser.  The URI
 * of the document is compared against aExpectedURL.  The load is then stopped
 * before it actually starts.
 *
 * @param aExpectedURL
 *        The URL of the document that is expected to load.
 * @param aStopFromProgressListener
 *        Whether to cancel the load directly from the progress listener. Defaults to true.
 *        If you're using this method to avoid hitting the network, you want the default (true).
 *        However, the browser UI will behave differently for loads stopped directly from
 *        the progress listener (effectively in the middle of a call to loadURI) and so there
 *        are cases where you may want to avoid stopping the load directly from within the
 *        progress listener callback.
 * @return promise
 */
function waitForDocLoadAndStopIt(aExpectedURL, aBrowser = gBrowser.selectedBrowser, aStopFromProgressListener = true) {
  function content_script(contentStopFromProgressListener) {
    ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
    let wp = docShell.QueryInterface(Ci.nsIWebProgress);

    function stopContent(now, uri) {
      if (now) {
        /* Hammer time. */
        content.stop();

        /* Let the parent know we're done. */
        sendAsyncMessage("Test:WaitForDocLoadAndStopIt", { uri });
      } else {
        setTimeout(stopContent.bind(null, true, uri), 0);
      }
    }

    let progressListener = {
      onStateChange(webProgress, req, flags, status) {
        dump("waitForDocLoadAndStopIt: onStateChange " + flags.toString(16) + ": " + req.name + "\n");

        if (webProgress.isTopLevel &&
            flags & Ci.nsIWebProgressListener.STATE_START) {
          wp.removeProgressListener(progressListener);

          let chan = req.QueryInterface(Ci.nsIChannel);
          dump(`waitForDocLoadAndStopIt: Document start: ${chan.URI.spec}\n`);

          stopContent(contentStopFromProgressListener, chan.originalURI.spec);
        }
      },
      QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),
    };
    wp.addProgressListener(progressListener, wp.NOTIFY_STATE_WINDOW);

    /**
     * As |this| is undefined and we can't extend |docShell|, adding an unload
     * event handler is the easiest way to ensure the weakly referenced
     * progress listener is kept alive as long as necessary.
     */
    addEventListener("unload", function() {
      try {
        wp.removeProgressListener(progressListener);
      } catch (e) { /* Will most likely fail. */ }
    });
  }

  // We are deferring the setup of this promise because there is a possibility
  // that a process flip will occur as we transition from page to page. This is
  // a little convoluted because we have a very small window of time in which to
  // send down the content_script frame script before the expected page actually
  // loads. The best time to send down the script, it seems, is right after the
  // TabRemotenessUpdate event.
  //
  // So, we abstract out the content_script handling into a helper stoppedDocLoadPromise
  // promise so that we can account for the process flipping case, and jam in the
  // content_script at just the right time in the TabRemotenessChange handler.
  let stoppedDocLoadPromise = () => {
    return new Promise((resolve, reject) => {
      function complete({ data }) {
        is(data.uri, aExpectedURL, "waitForDocLoadAndStopIt: The expected URL was loaded");
        mm.removeMessageListener("Test:WaitForDocLoadAndStopIt", complete);
        resolve();
      }

      let mm = aBrowser.messageManager;
      mm.loadFrameScript("data:,(" + content_script.toString() + ")(" + aStopFromProgressListener + ");", true);
      mm.addMessageListener("Test:WaitForDocLoadAndStopIt", complete);
      info("waitForDocLoadAndStopIt: Waiting for URL: " + aExpectedURL);
    });
  };

  let win = aBrowser.ownerGlobal;
  let tab = win.gBrowser.getTabForBrowser(aBrowser);
  let { mustChangeProcess } = E10SUtils.shouldLoadURIInBrowser(aBrowser, aExpectedURL);
  if (!tab ||
      !win.gMultiProcessBrowser ||
      !mustChangeProcess) {
    return stoppedDocLoadPromise();
  }

  return new Promise((resolve, reject) => {
    tab.addEventListener("TabRemotenessChange", function() {
      stoppedDocLoadPromise().then(resolve, reject);
    }, {once: true});
  });
}

/**
 * Wait for the search engine to change.
 */
function promiseContentSearchChange(browser, newEngineName) {
  return ContentTask.spawn(browser, { newEngineName }, async function(args) {
    return new Promise(resolve => {
      content.addEventListener("ContentSearchService", function listener(aEvent) {
        if (aEvent.detail.type == "CurrentState" &&
            content.wrappedJSObject.gContentSearchController.defaultEngine.name == args.newEngineName) {
          content.removeEventListener("ContentSearchService", listener);
          resolve();
        }
      });
    });
  });
}

/**
 * Wait for the search engine to be added.
 */
function promiseNewEngine(basename) {
  info("Waiting for engine to be added: " + basename);
  return new Promise((resolve, reject) => {
    let url = getRootDirectory(gTestPath) + basename;
    Services.search.addEngine(url, "", false, {
      onSuccess(engine) {
        info("Search engine added: " + basename);
        registerCleanupFunction(() => {
          try {
            Services.search.removeEngine(engine);
          } catch (ex) { /* Can't remove the engine more than once */ }
        });
        resolve(engine);
      },
      onError(errCode) {
        ok(false, "addEngine failed with error code " + errCode);
        reject();
      },
    });
  });
}

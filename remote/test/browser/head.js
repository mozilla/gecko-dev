/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteAgent } = ChromeUtils.import(
  "chrome://remote/content/RemoteAgent.jsm"
);
const { RemoteAgentError } = ChromeUtils.import(
  "chrome://remote/content/Error.jsm"
);

/**
 * Override `add_task` in order to translate chrome-remote-interface exceptions
 * into something that logs better errors on stdout
 */
const add_plain_task = add_task.bind(this);
this.add_task = function(taskFn, opts = {}) {
  const { createTab = true } = opts;

  add_plain_task(async function() {
    info("Start the CDP server");
    await RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

    try {
      const CDP = await getCDP();

      // By default run each test in its own tab
      if (createTab) {
        const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
        const browsingContextId = tab.linkedBrowser.browsingContext.id;

        const client = await CDP({
          target(list) {
            return list.find(target => target.id === browsingContextId);
          },
        });
        info("CDP client instantiated");

        await taskFn(client, CDP, tab);

        // taskFn may resolve within a tick after opening a new tab.
        // We shouldn't remove the newly opened tab in the same tick.
        // Wait for the next tick here.
        await TestUtils.waitForTick();
        BrowserTestUtils.removeTab(tab);

        await client.close();
        info("CDP client closed");
      } else {
        const client = await CDP({});
        await taskFn(client, CDP);
      }
    } catch (e) {
      // Display better error message with the server side stacktrace
      // if an error happened on the server side:
      if (e.response) {
        throw RemoteAgentError.fromJSON(e.response);
      } else {
        throw e;
      }
    } finally {
      info("Stop the CDP server");
      await RemoteAgent.close();

      // Close any additional tabs, so that only a single tab remains open
      while (gBrowser.tabs.length > 1) {
        gBrowser.removeCurrentTab();
      }
    }
  });
};

const CRI_URI =
  "http://example.com/browser/remote/test/browser/chrome-remote-interface.js";

/**
 * Create a test document in an invisible window.
 * This window will be automatically closed on test teardown.
 */
function createTestDocument() {
  const browser = Services.appShell.createWindowlessBrowser(true);
  const webNavigation = browser.docShell.QueryInterface(Ci.nsIWebNavigation);
  // Create a system principal content viewer to ensure there is a valid
  // empty document using system principal and avoid any wrapper issues
  // when using document's JS Objects.
  const system = Services.scriptSecurityManager.getSystemPrincipal();
  webNavigation.createAboutBlankContentViewer(system, system);

  registerCleanupFunction(() => browser.close());
  return webNavigation.document;
}

/**
 * Retrieve an intance of CDP object from chrome-remote-interface library
 */
async function getCDP() {
  // Instantiate a background test document in order to load the library
  // as in a web page
  const document = createTestDocument();

  // Load chrome-remote-interface.js into this background test document
  const script = document.createElement("script");
  script.setAttribute("src", CRI_URI);
  document.documentElement.appendChild(script);
  await new Promise(resolve => {
    script.addEventListener("load", resolve, { once: true });
  });

  const window = document.defaultView.wrappedJSObject;

  // Implements `criRequest` to be called by chrome-remeote-interface
  // library in order to do the cross-domain http request, which,
  // in a regular Web page, is impossible.
  window.criRequest = (options, callback) => {
    const { host, port, path } = options;
    const url = `http://${host}:${port}${path}`;
    const xhr = new XMLHttpRequest();
    xhr.open("GET", url, true);

    // Prevent "XML Parsing Error: syntax error" error messages
    xhr.overrideMimeType("text/plain");

    xhr.send(null);
    xhr.onload = () => callback(null, xhr.responseText);
    xhr.onerror = e => callback(e, null);
  };

  return window.CDP;
}

function getTargets(CDP) {
  return new Promise((resolve, reject) => {
    CDP.List(null, (err, targets) => {
      if (err) {
        reject(err);
        return;
      }
      resolve(targets);
    });
  });
}

/** Creates a data URL for the given source document. */
function toDataURL(src, doctype = "html") {
  let doc, mime;
  switch (doctype) {
    case "html":
      mime = "text/html;charset=utf-8";
      doc = `<!doctype html>\n<meta charset=utf-8>\n${src}`;
      break;
    default:
      throw new Error("Unexpected doctype: " + doctype);
  }

  return `data:${mime},${encodeURIComponent(doc)}`;
}

/**
 * Load a given URL in the currently selected tab
 */
async function loadURL(url) {
  const browser = gBrowser.selectedTab.linkedBrowser;
  const loaded = BrowserTestUtils.browserLoaded(browser, false, url);

  BrowserTestUtils.loadURI(browser, url);
  await loaded;
}

/**
 * Retrieve the value of a property on the content window.
 */
function getContentProperty(prop) {
  info(`Retrieve ${prop} on the content window`);
  return ContentTask.spawn(
    gBrowser.selectedBrowser,
    prop,
    _prop => content[_prop]
  );
}

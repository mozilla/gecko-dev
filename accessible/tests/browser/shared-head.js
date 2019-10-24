/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../mochitest/common.js */
/* import-globals-from ../mochitest/promisified-events.js */

/* exported Logger, MOCHITESTS_DIR, invokeSetAttribute, invokeFocus,
            invokeSetStyle, getAccessibleDOMNodeID, getAccessibleTagName,
            addAccessibleTask, findAccessibleChildByID, isDefunct,
            CURRENT_CONTENT_DIR, loadScripts, loadContentScripts, snippetToURL,
            Cc, Cu, arrayFromChildren, forceGC, contentSpawnMutation,
            FISSION_IFRAME_ID, DEFAULT_FISSION_DOC_BODY_ID, invokeContentTask,
            matchContentDoc, currentContentDoc */

const CURRENT_FILE_DIR = "/browser/accessible/tests/browser/";

/**
 * Current browser test directory path used to load subscripts.
 */
const CURRENT_DIR = `chrome://mochitests/content${CURRENT_FILE_DIR}`;
/**
 * A11y mochitest directory where we find common files used in both browser and
 * plain tests.
 */
const MOCHITESTS_DIR =
  "chrome://mochitests/content/a11y/accessible/tests/mochitest/";
/**
 * A base URL for test files used in content.
 */
const CURRENT_CONTENT_DIR = `http://example.com${CURRENT_FILE_DIR}`;

const LOADED_CONTENT_SCRIPTS = new Map();

const DEFAULT_CONTENT_DOC_BODY_ID = "body";
const FISSION_IFRAME_ID = "fission-iframe";
const DEFAULT_FISSION_DOC_BODY_ID = "fission-body";

let gIsFission = false;

function currentContentDoc() {
  return gIsFission ? DEFAULT_FISSION_DOC_BODY_ID : DEFAULT_CONTENT_DOC_BODY_ID;
}

/**
 * Accessible event match criteria based on the id of the current document
 * accessible in test.
 *
 * @param   {nsIAccessibleEvent}  event
 *        Accessible event to be tested for a match.
 *
 * @return  {Boolean}
 *          True if accessible event's accessible object ID matches current
 *          document accessible ID.
 */
function matchContentDoc(event) {
  return getAccessibleDOMNodeID(event.accessible) === currentContentDoc();
}

/**
 * Used to dump debug information.
 */
let Logger = {
  /**
   * Set up this variable to dump log messages into console.
   */
  dumpToConsole: false,

  /**
   * Set up this variable to dump log messages into error console.
   */
  dumpToAppConsole: false,

  /**
   * Return true if dump is enabled.
   */
  get enabled() {
    return this.dumpToConsole || this.dumpToAppConsole;
  },

  /**
   * Dump information into console if applicable.
   */
  log(msg) {
    if (this.enabled) {
      this.logToConsole(msg);
      this.logToAppConsole(msg);
    }
  },

  /**
   * Log message to console.
   */
  logToConsole(msg) {
    if (this.dumpToConsole) {
      dump(`\n${msg}\n`);
    }
  },

  /**
   * Log message to error console.
   */
  logToAppConsole(msg) {
    if (this.dumpToAppConsole) {
      Services.console.logStringMessage(`${msg}`);
    }
  },
};

/**
 * Asynchronously set or remove content element's attribute (in content process
 * if e10s is enabled).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @param  {String}  attr     attribute name
 * @param  {String?} value    optional attribute value, if not present, remove
 *                            attribute
 * @return {Promise}          promise indicating that attribute is set/removed
 */
function invokeSetAttribute(browser, id, attr, value) {
  if (value) {
    Logger.log(`Setting ${attr} attribute to ${value} for node with id: ${id}`);
  } else {
    Logger.log(`Removing ${attr} attribute from node with id: ${id}`);
  }

  return invokeContentTask(
    browser,
    [id, attr, value],
    (contentId, contentAttr, contentValue) => {
      let elm = content.document.getElementById(contentId);
      if (contentValue) {
        elm.setAttribute(contentAttr, contentValue);
      } else {
        elm.removeAttribute(contentAttr);
      }
    }
  );
}

/**
 * Asynchronously set or remove content element's style (in content process if
 * e10s is enabled, or in fission process if fission is enabled and a fission
 * frame is present).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @param  {String}  aStyle   style property name
 * @param  {String?} aValue   optional style property value, if not present,
 *                            remove style
 * @return {Promise}          promise indicating that style is set/removed
 */
function invokeSetStyle(browser, id, style, value) {
  if (value) {
    Logger.log(`Setting ${style} style to ${value} for node with id: ${id}`);
  } else {
    Logger.log(`Removing ${style} style from node with id: ${id}`);
  }

  return invokeContentTask(
    browser,
    [id, style, value],
    (contentId, contentStyle, contentValue) => {
      const elm = content.document.getElementById(contentId);
      if (contentValue) {
        elm.style[contentStyle] = contentValue;
      } else {
        delete elm.style[contentStyle];
      }
    }
  );
}

/**
 * Asynchronously set focus on a content element (in content process if e10s is
 * enabled, or in fission process if fission is enabled and a fission frame is
 * present).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @return {Promise} promise  indicating that focus is set
 */
function invokeFocus(browser, id) {
  Logger.log(`Setting focus on a node with id: ${id}`);

  return invokeContentTask(browser, [id], contentId => {
    const elm = content.document.getElementById(contentId);
    if (elm.editor) {
      elm.selectionStart = elm.selectionEnd = elm.value.length;
    }

    elm.focus();
  });
}

/**
 * Asynchronously perform a task in content (in content process if e10s is
 * enabled, or in fission process if fission is enabled and a fission frame is
 * present).
 * @param  {Object}    browser  current "tabbrowser" element
 * @param  {Array}     args     arguments for the content task
 * @param  {Function}  task     content task function
 *
 * @return {Promise} promise  indicating that content task is complete
 */
function invokeContentTask(browser, args, task) {
  return SpecialPowers.spawn(
    browser,
    [FISSION_IFRAME_ID, task.toString(), ...args],
    (fissionFrameId, contentTask, ...contentArgs) => {
      // eslint-disable-next-line no-eval
      const runnableTask = eval(`
      (() => {
        return (${contentTask});
      })();`);
      const frame = content.document.getElementById(fissionFrameId);

      return frame
        ? SpecialPowers.spawn(frame, contentArgs, runnableTask)
        : runnableTask.call(this, ...contentArgs);
    }
  );
}

/**
 * Load a list of scripts into the test
 * @param {Array} scripts  a list of scripts to load
 */
function loadScripts(...scripts) {
  for (let script of scripts) {
    let path =
      typeof script === "string"
        ? `${CURRENT_DIR}${script}`
        : `${script.dir}${script.name}`;
    Services.scriptloader.loadSubScript(path, this);
  }
}

/**
 * Load a list of scripts into target's content.
 * @param {Object} target
 *        target for loading scripts into
 * @param {Array}  scripts
 *        a list of scripts to load into content
 */
async function loadContentScripts(target, ...scripts) {
  for (let script of scripts) {
    let contentScript;
    if (typeof script === "string") {
      // If script string includes a .jsm extention, assume it is a module path.
      contentScript = `${CURRENT_DIR}${script}`;
    } else {
      // Script is a object that has { dir, name } format.
      contentScript = `${script.dir}${script.name}`;
    }

    let loadedScriptSet = LOADED_CONTENT_SCRIPTS.get(contentScript);
    if (!loadedScriptSet) {
      loadedScriptSet = new WeakSet();
      LOADED_CONTENT_SCRIPTS.set(contentScript, loadedScriptSet);
    } else if (loadedScriptSet.has(target)) {
      continue;
    }

    await SpecialPowers.spawn(target, [contentScript], async _contentScript => {
      ChromeUtils.import(_contentScript, content.window);
    });
    loadedScriptSet.add(target);
  }
}

function attrsToString(attrs) {
  return Object.entries(attrs)
    .map(([attr, value]) => `${attr}=${JSON.stringify(value)}`)
    .join(" ");
}

function wrapWithFissionIFrame(doc, options = {}) {
  const srcURL = new URL(`${CURRENT_CONTENT_DIR}fission_document_builder.sjs`);
  if (doc.endsWith("html")) {
    srcURL.searchParams.append("file", `${CURRENT_FILE_DIR}e10s/${doc}`);
  } else {
    const { fissionDocBodyAttrs = {} } = options;
    const attrs = {
      id: DEFAULT_FISSION_DOC_BODY_ID,
      ...fissionDocBodyAttrs,
    };

    srcURL.searchParams.append(
      "html",
      `<html>
        <head>
          <meta charset="utf-8"/>
          <title>Accessibility Fission Test</title>
        </head>
        <body ${attrsToString(attrs)}>${doc}</body>
      </html>`
    );
  }

  return `<iframe id="${FISSION_IFRAME_ID}" src="${srcURL.href}"/>`;
}

/**
 * Takes an HTML snippet or HTML doc url and returns an encoded URI for a full
 * document with the snippet or the URL as a source for the IFRAME.
 * @param {String} doc
 *        a markup snippet or url.
 * @param {Object} options
 *        additonal options:
 *        - {Boolean} fission
 *          true if the content document should be wrapped inside a fission
 *          iframe
 *        - {Object} contentDocBodyAttrs
 *          a set of attributes to be applied to a top level content document
 *          body
 *        - {Object} fissionDocBodyAttrs
 *          a set of attributes to be applied to a fission content document body
 * @return {String}
 *        a base64 encoded data url of the document container the snippet.
 **/
function snippetToURL(doc, options = {}) {
  const { contentDocBodyAttrs = {} } = options;
  const attrs = {
    id: DEFAULT_CONTENT_DOC_BODY_ID,
    ...contentDocBodyAttrs,
  };

  if (options.fission) {
    doc = wrapWithFissionIFrame(doc, options);
  }

  const encodedDoc = encodeURIComponent(
    `<html>
      <head>
        <meta charset="utf-8"/>
        <title>Accessibility Test</title>
      </head>
      <body ${attrsToString(attrs)}>${doc}</body>
    </html>`
  );

  return `data:text/html;charset=utf-8,${encodedDoc}`;
}

function accessibleTask(doc, task, options = {}) {
  return async function() {
    let url;
    if (doc.endsWith("html") && !options.fission) {
      url = `${CURRENT_CONTENT_DIR}e10s/${doc}`;
    } else {
      url = snippetToURL(doc, options);
    }

    registerCleanupFunction(() => {
      for (let observer of Services.obs.enumerateObservers(
        "accessible-event"
      )) {
        Services.obs.removeObserver(observer, "accessible-event");
      }
    });

    const onContentDocLoad = waitForEvent(
      EVENT_DOCUMENT_LOAD_COMPLETE,
      DEFAULT_CONTENT_DOC_BODY_ID
    );

    let onFissionDocLoad;
    if (options.fission) {
      gIsFission = true;
      if (gFissionBrowser) {
        onFissionDocLoad = waitForEvent(
          EVENT_DOCUMENT_LOAD_COMPLETE,
          DEFAULT_FISSION_DOC_BODY_ID
        );
      }
    }

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url,
      },
      async function(browser) {
        registerCleanupFunction(() => {
          if (browser) {
            let tab = gBrowser.getTabForBrowser(browser);
            if (tab && !tab.closing && tab.linkedBrowser) {
              gBrowser.removeTab(tab);
            }
          }
        });

        await SimpleTest.promiseFocus(browser);
        await loadContentScripts(browser, "Common.jsm");

        Logger.log(
          `e10s enabled: ${Services.appinfo.browserTabsRemoteAutostart}`
        );
        Logger.log(`Actually remote browser: ${browser.isRemoteBrowser}`);

        const { accessible: docAccessible } = await onContentDocLoad;
        let fissionDocAccessible;
        if (options.fission) {
          fissionDocAccessible = gFissionBrowser
            ? (await onFissionDocLoad).accessible
            : findAccessibleChildByID(docAccessible, FISSION_IFRAME_ID)
                .firstChild;
        }

        await task(
          browser,
          fissionDocAccessible || docAccessible,
          fissionDocAccessible && docAccessible
        );
      }
    );
  };
}

/**
 * A wrapper around browser test add_task that triggers an accessible test task
 * as a new browser test task with given document, data URL or markup snippet.
 * @param  {String} doc
 *         URL (relative to current directory) or data URL or markup snippet
 *         that is used to test content with
 * @param  {Function|AsyncFunction} task
 *         a generator or a function with tests to run
 * @param  {null|Object} options
 *         Options for running accessibility test tasks:
 *         - {Boolean} topLevel
 *           Flag to run the test with content in the top level content process.
 *           Default is true.
 *         - {Boolean} iframe
 *           Flag to run the test with content wrapped in an iframe. Default is
 *           false.
 */
function addAccessibleTask(
  doc,
  task,
  { topLevel = true, iframe = false } = {}
) {
  if (topLevel) {
    add_task(accessibleTask(doc, task));
  }

  if (iframe) {
    add_task(accessibleTask(doc, task, { fission: true }));
  }
}

/**
 * Check if an accessible object has a defunct test.
 * @param  {nsIAccessible}  accessible object to test defunct state for
 * @return {Boolean}        flag indicating defunct state
 */
function isDefunct(accessible) {
  let defunct = false;
  try {
    let extState = {};
    accessible.getState({}, extState);
    defunct = extState.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT;
  } catch (x) {
    defunct = true;
  } finally {
    if (defunct) {
      Logger.log(`Defunct accessible: ${prettyName(accessible)}`);
    }
  }
  return defunct;
}

/**
 * Get the DOM tag name for a given accessible.
 * @param  {nsIAccessible}  accessible accessible
 * @return {String?}                   tag name of associated DOM node, or null.
 */
function getAccessibleTagName(acc) {
  try {
    return acc.attributes.getStringProperty("tag");
  } catch (e) {
    return null;
  }
}

/**
 * Traverses the accessible tree starting from a given accessible as a root and
 * looks for an accessible that matches based on its DOMNode id.
 * @param  {nsIAccessible}  accessible root accessible
 * @param  {String}         id         id to look up accessible for
 * @param  {Array?}         interfaces the interface or an array interfaces
 *                                     to query it/them from obtained accessible
 * @return {nsIAccessible?}            found accessible if any
 */
function findAccessibleChildByID(accessible, id, interfaces) {
  if (getAccessibleDOMNodeID(accessible) === id) {
    return queryInterfaces(accessible, interfaces);
  }
  for (let i = 0; i < accessible.children.length; ++i) {
    let found = findAccessibleChildByID(accessible.getChildAt(i), id);
    if (found) {
      return queryInterfaces(found, interfaces);
    }
  }
  return null;
}

function queryInterfaces(accessible, interfaces) {
  if (!interfaces) {
    return accessible;
  }

  for (let iface of interfaces.filter(i => !(accessible instanceof i))) {
    try {
      accessible.QueryInterface(iface);
    } catch (e) {
      ok(false, "Can't query " + iface);
    }
  }

  return accessible;
}

function arrayFromChildren(accessible) {
  return Array.from({ length: accessible.childCount }, (c, i) =>
    accessible.getChildAt(i)
  );
}

/**
 * Force garbage collection.
 */
function forceGC() {
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
}

/*
 * This function spawns a content task and awaits expected mutation events from
 * various content changes. It's good at catching events we did *not* expect. We
 * do this advancing the layout refresh to flush the relocations/insertions
 * queue.
 */
async function contentSpawnMutation(browser, waitFor, func, args = []) {
  let onReorders = waitForEvents({ expected: waitFor.expected || [] });
  let unexpectedListener = new UnexpectedEvents(waitFor.unexpected || []);

  function tick() {
    // 100ms is an arbitrary positive number to advance the clock.
    // We don't need to advance the clock for a11y mutations, but other
    // tick listeners may depend on an advancing clock with each refresh.
    content.windowUtils.advanceTimeAndRefresh(100);
  }

  // This stops the refreh driver from doing its regular ticks, and leaves
  // us in control.
  await invokeContentTask(browser, [], tick);

  // Perform the tree mutation.
  await invokeContentTask(browser, args, func);

  // Do one tick to flush our queue (insertions, relocations, etc.)
  await invokeContentTask(browser, [], tick);

  let events = await onReorders;

  unexpectedListener.stop();

  // Go back to normal refresh driver ticks.
  await invokeContentTask(browser, [], function() {
    content.windowUtils.restoreNormalRefresh();
  });

  return events;
}

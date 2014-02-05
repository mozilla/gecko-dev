/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that starting a load invalidates shistory.
 */
add_task(function test_load_start() {
  // Create a new tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Load a new URI but remove the tab before it has finished loading.
  browser.loadURI("about:mozilla");
  yield promiseContentMessage(browser, "ss-test:onFrameTreeReset");
  gBrowser.removeTab(tab);

  // Undo close the tab.
  tab = ss.undoCloseTab(window, 0);
  browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Check that the correct URL was restored.
  is(browser.currentURI.spec, "about:mozilla", "url is correct");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Ensure that purging shistory invalidates.
 */
add_task(function test_purge() {
  // Create a new tab.
  let tab = gBrowser.addTab("about:mozilla");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Create a second shistory entry.
  browser.loadURI("about:robots");
  yield promiseBrowserLoaded(browser);

  // Check that we now have two shistory entries.
  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 2, "there are two shistory entries");

  // Purge session history.
  yield sendMessage(browser, "ss-test:purgeSessionHistory");

  // Check that we are left with a single shistory entry.
  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is one shistory entry");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Ensure that anchor navigation invalidates shistory.
 */
add_task(function test_hashchange() {
  const URL = "data:text/html;charset=utf-8,<a id=a href=%23>clickme</a>";

  // Create a new tab.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Check that we start with a single shistory entry.
  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is one shistory entry");

  // Click the link and wait for a hashchange event.
  browser.messageManager.sendAsyncMessage("ss-test:click", {id: "a"});
  yield promiseContentMessage(browser, "ss-test:hashchange");

  // Check that we now have two shistory entries.
  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 2, "there are two shistory entries");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Ensure that loading pages from the bfcache invalidates shistory.
 */
add_task(function test_pageshow() {
  const URL = "data:text/html;charset=utf-8,<h1>first</h1>";
  const URL2 = "data:text/html;charset=utf-8,<h1>second</h1>";

  // Create a new tab.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Create a second shistory entry.
  browser.loadURI(URL2);
  yield promiseBrowserLoaded(browser);

  // Go back to the previous url which is loaded from the bfcache.
  browser.goBack();
  yield promiseContentMessage(browser, "ss-test:onFrameTreeCollected");
  is(browser.currentURI.spec, URL, "correct url after going back");

  // Check that loading from bfcache did invalidate shistory.
  SyncHandlers.get(browser).flush();
  let {index} = JSON.parse(ss.getTabState(tab));
  is(index, 1, "first history entry is selected");

  // Cleanup.
  gBrowser.removeTab(tab);
});

/**
 * Ensure that subframe navigation invalidates shistory.
 */
add_task(function test_subframes() {
  const URL = "data:text/html;charset=utf-8," +
              "<iframe src=http%3A//example.com/ name=t></iframe>" +
              "<a id=a1 href=http%3A//example.com/1 target=t>clickme</a>" +
              "<a id=a2 href=http%3A//example.com/%23 target=t>clickme</a>";

  // Create a new tab.
  let tab = gBrowser.addTab(URL);
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Check that we have a single shistory entry.
  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is one shistory entry");
  is(entries[0].children.length, 1, "the entry has one child");

  // Navigate the subframe.
  browser.messageManager.sendAsyncMessage("ss-test:click", {id: "a1"});
  yield promiseBrowserLoaded(browser, false /* don't ignore subframes */);

  // Check shistory.
  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 2, "there now are two shistory entries");
  is(entries[1].children.length, 1, "the second entry has one child");

  // Go back in history.
  browser.goBack();
  yield promiseBrowserLoaded(browser, false /* don't ignore subframes */);

  // Navigate the subframe again.
  browser.messageManager.sendAsyncMessage("ss-test:click", {id: "a2"});
  yield promiseContentMessage(browser, "ss-test:hashchange");

  // Check shistory.
  SyncHandlers.get(browser).flush();
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 2, "there now are two shistory entries");
  is(entries[1].children.length, 1, "the second entry has one child");

  // Cleanup.
  gBrowser.removeTab(tab);
});

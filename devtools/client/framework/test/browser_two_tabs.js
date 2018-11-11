/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check regression when opening two tabs
 */

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");

const TAB_URL_1 = "data:text/html;charset=utf-8,foo";
const TAB_URL_2 = "data:text/html;charset=utf-8,bar";

var gClient;
var gTab1, gTab2;
var gTargetActor1, gTargetActor2;

function test() {
  waitForExplicitFinish();

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  openTabs();
}

function openTabs() {
  // Open two tabs, select the second
  addTab(TAB_URL_1).then(tab1 => {
    gTab1 = tab1;
    addTab(TAB_URL_2).then(tab2 => {
      gTab2 = tab2;

      connect();
    });
  });
}

function connect() {
  // Connect to debugger server to fetch the two target actors for each tab
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect()
    .then(() => gClient.listTabs())
    .then(response => {
      // Fetch the target actors for each tab
      gTargetActor1 = response.tabs.filter(a => a.url === TAB_URL_1)[0];
      gTargetActor2 = response.tabs.filter(a => a.url === TAB_URL_2)[0];

      checkGetTab();
    });
}

function checkGetTab() {
  gClient.getTab({tab: gTab1})
         .then(response => {
           is(JSON.stringify(gTargetActor1), JSON.stringify(response.tab),
              "getTab returns the same target form for first tab");
         })
         .then(() => {
           const filter = {};
           // Filter either by tabId or outerWindowID,
           // if we are running tests OOP or not.
           if (gTab1.linkedBrowser.frameLoader.tabParent) {
             filter.tabId = gTab1.linkedBrowser.frameLoader.tabParent.tabId;
           } else {
             const windowUtils = gTab1.linkedBrowser.contentWindow.windowUtils;
             filter.outerWindowID = windowUtils.outerWindowID;
           }
           return gClient.getTab(filter);
         })
         .then(response => {
           is(JSON.stringify(gTargetActor1), JSON.stringify(response.tab),
              "getTab returns the same target form when filtering by tabId/outerWindowID");
         })
         .then(() => gClient.getTab({tab: gTab2}))
         .then(response => {
           is(JSON.stringify(gTargetActor2), JSON.stringify(response.tab),
              "getTab returns the same target form for second tab");
         })
         .then(checkGetTabFailures);
}

function checkGetTabFailures() {
  gClient.getTab({ tabId: -999 })
    .then(
      response => ok(false, "getTab unexpectedly succeed with a wrong tabId"),
      response => {
        is(response, "Protocol error (noTab): Unable to find tab with tabId '-999'");
      }
    )
    .then(() => gClient.getTab({ outerWindowID: -999 }))
    .then(
      response => ok(false, "getTab unexpectedly succeed with a wrong outerWindowID"),
      response => {
        is(response, "Protocol error (noTab): Unable to find tab with outerWindowID '-999'");
      }
    )
    .then(checkSelectedTargetActor);
}

function checkSelectedTargetActor() {
  // Send a naive request to the second target actor to check if it works
  gClient.request({ to: gTargetActor2.consoleActor, type: "startListeners", listeners: [] }, aResponse => {
    ok("startedListeners" in aResponse, "Actor from the selected tab should respond to the request.");

    closeSecondTab();
  });
}

function closeSecondTab() {
  // Close the second tab, currently selected
  const container = gBrowser.tabContainer;
  container.addEventListener("TabClose", function() {
    checkFirstTargetActor();
  }, {once: true});
  gBrowser.removeTab(gTab2);
}

function checkFirstTargetActor() {
  // then send a request to the first target actor to check if it still works
  gClient.request({ to: gTargetActor1.consoleActor, type: "startListeners", listeners: [] }, aResponse => {
    ok("startedListeners" in aResponse, "Actor from the first tab should still respond.");

    cleanup();
  });
}

function cleanup() {
  const container = gBrowser.tabContainer;
  container.addEventListener("TabClose", function() {
    gClient.close().then(finish);
  }, {once: true});
  gBrowser.removeTab(gTab1);
}

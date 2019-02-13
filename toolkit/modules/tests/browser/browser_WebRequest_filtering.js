"use strict";

const { interfaces: Ci, classes: Cc, utils: Cu, results: Cr } = Components;

let {WebRequest} = Cu.import("resource://gre/modules/WebRequest.jsm", {});
let {MatchPattern} = Cu.import("resource://gre/modules/MatchPattern.jsm", {});

const BASE = "http://example.com/browser/toolkit/modules/tests/browser";
const URL = BASE + "/file_WebRequest_page2.html";

let requested = [];

function onBeforeRequest(details)
{
  info(`onBeforeRequest ${details.url}`);
  if (details.url.startsWith(BASE)) {
    requested.push(details.url);
  }
}

let sendHeaders = [];

function onBeforeSendHeaders(details)
{
  info(`onBeforeSendHeaders ${details.url}`);
  if (details.url.startsWith(BASE)) {
    sendHeaders.push(details.url);
  }
}

let completed = [];

function onResponseStarted(details)
{
  if (details.url.startsWith(BASE)) {
    completed.push(details.url);
  }
}

const expected_urls = [BASE + "/file_style_good.css",
                       BASE + "/file_style_bad.css",
                       BASE + "/file_style_redirect.css"];

function removeDupes(list)
{
  let j = 0;
  for (let i = 1; i < list.length; i++) {
    if (list[i] != list[j]) {
      j++;
      if (i != j) {
        list[j] = list[i];
      }
    }
  }
  list.length = j + 1;
}

function compareLists(list1, list2, kind)
{
  list1.sort();
  removeDupes(list1);
  list2.sort();
  removeDupes(list2);
  is(String(list1), String(list2), `${kind} URLs correct`);
}

add_task(function* filter_urls() {
  let filter = {urls: new MatchPattern("*://*/*_style_*")};

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, ["blocking"]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  gBrowser.selectedTab = gBrowser.addTab(URL);

  yield waitForLoad();

  gBrowser.removeCurrentTab();

  compareLists(requested, expected_urls, "requested");
  compareLists(sendHeaders, expected_urls, "sendHeaders");
  compareLists(completed, expected_urls, "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});

add_task(function* filter_types() {
  let filter = {types: ["stylesheet"]};

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, ["blocking"]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  gBrowser.selectedTab = gBrowser.addTab(URL);

  yield waitForLoad();

  gBrowser.removeCurrentTab();

  compareLists(requested, expected_urls, "requested");
  compareLists(sendHeaders, expected_urls, "sendHeaders");
  compareLists(completed, expected_urls, "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});

function waitForLoad(browser = gBrowser.selectedBrowser) {
  return new Promise(resolve => {
    browser.addEventListener("load", function listener() {
      browser.removeEventListener("load", listener, true);
      resolve();
    }, true);
  });
}

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/common/utils.js");

function getMockStore() {
  let engine = new TabEngine(Service);
  let store = engine._store;
  store.getTabState = mockGetTabState;
  store.shouldSkipWindow = mockShouldSkipWindow;
  return store;
}

function test_create() {
  let store = new TabEngine(Service)._store;

  _("Create a first record");
  let rec = {id: "id1",
             clientName: "clientName1",
             cleartext: "cleartext1",
             modified: 1000};
  store.applyIncoming(rec);
  do_check_eq(store._remoteClients["id1"], "cleartext1");
  do_check_eq(Svc.Prefs.get("notifyTabState"), 1);

  _("Create a second record");
  rec = {id: "id2",
         clientName: "clientName2",
         cleartext: "cleartext2",
         modified: 2000};
  store.applyIncoming(rec);
  do_check_eq(store._remoteClients["id2"], "cleartext2");
  do_check_eq(Svc.Prefs.get("notifyTabState"), 0);

  _("Create a third record");
  rec = {id: "id3",
         clientName: "clientName3",
         cleartext: "cleartext3",
         modified: 3000};
  store.applyIncoming(rec);
  do_check_eq(store._remoteClients["id3"], "cleartext3");
  do_check_eq(Svc.Prefs.get("notifyTabState"), 0);

  // reset the notifyTabState
  Svc.Prefs.reset("notifyTabState");
}

function test_getAllTabs() {
  let store = getMockStore();
  let tabs;

  let threeUrls = ["http://foo.com", "http://fuubar.com", "http://barbar.com"];

  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, "http://bar.com", 1, 1, () => 2, () => threeUrls);

  _("Get all tabs.");
  tabs = store.getAllTabs();
  _("Tabs: " + JSON.stringify(tabs));
  do_check_eq(tabs.length, 1);
  do_check_eq(tabs[0].title, "title");
  do_check_eq(tabs[0].urlHistory.length, 2);
  do_check_eq(tabs[0].urlHistory[0], "http://foo.com");
  do_check_eq(tabs[0].urlHistory[1], "http://bar.com");
  do_check_eq(tabs[0].icon, "image");
  do_check_eq(tabs[0].lastUsed, 1);

  _("Get all tabs, and check that filtering works.");
  let twoUrls = ["about:foo", "http://fuubar.com"];
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, "http://foo.com", 1, 1, () => 2, () => twoUrls);
  tabs = store.getAllTabs(true);
  _("Filtered: " + JSON.stringify(tabs));
  do_check_eq(tabs.length, 0);

  _("Get all tabs, and check that the entries safety limit works.");
  let allURLs = [];
  for (let i = 0; i < 50; i++) {
    allURLs.push("http://foo" + i + ".bar");
  }
  allURLs.splice(35, 0, "about:foo", "about:bar", "about:foobar");

  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, "http://bar.com", 1, 1, () => 45, () => allURLs);
  tabs = store.getAllTabs((url) => url.startsWith("about"));

  _("Sliced: " + JSON.stringify(tabs));
  do_check_eq(tabs.length, 1);
  do_check_eq(tabs[0].urlHistory.length, 25);
  do_check_eq(tabs[0].urlHistory[0], "http://foo40.bar");
  do_check_eq(tabs[0].urlHistory[24], "http://foo16.bar");
}

function test_createRecord() {
  let store = getMockStore();
  let record;

  store.getTabState = mockGetTabState;
  store.shouldSkipWindow = mockShouldSkipWindow;
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, "http://foo.com", 1, 1);

  let tabs = store.getAllTabs();
  let tabsize = JSON.stringify(tabs[0]).length;
  let numtabs = Math.ceil(20000./77.);

  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, "http://foo.com", 1, 1);
  record = store.createRecord("fake-guid");
  do_check_true(record instanceof TabSetRecord);
  do_check_eq(record.tabs.length, 1);

  _("create a big record");
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, "http://foo.com", 1, numtabs);
  record = store.createRecord("fake-guid");
  do_check_true(record instanceof TabSetRecord);
  do_check_eq(record.tabs.length, 256);
}

function run_test() {
  test_create();
  test_getAllTabs();
  test_createRecord();
}

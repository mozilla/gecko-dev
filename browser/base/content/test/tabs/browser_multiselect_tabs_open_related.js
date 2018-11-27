const PREF_MULTISELECT_TABS = "browser.tabs.multiselect";

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_MULTISELECT_TABS, true],
    ],
  });

  let tab1 = await addTab("http://example.com/1");
  let tab2 = await addTab("http://example.com/2");
  let tab3 = await addTab("http://example.com/3");


  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, {ctrlKey: true});

  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(tab2.multiselected, "Tab2 is multi-selected");
  ok(!tab3.multiselected, "Tab3 is not multi-selected");

  let metaKeyEvent = AppConstants.platform == "macosx" ? {metaKey: true} : {ctrlKey: true};

  let tabs = document.getElementById("tabbrowser-tabs");
  let newTabButton = document.getAnonymousElementByAttribute(tabs, "anonid", "tabs-newtab-button");
  let promiseTabOpened = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(newTabButton, metaKeyEvent);
  let openEvent = await promiseTabOpened;
  let newTab = openEvent.target;

  is(newTab.previousElementSibling, tab2,
    "New tab should be opened after tab2 when tab1 and tab2 are multiselected");
  is(newTab.nextElementSibling, tab3,
    "New tab should be opened before tab3 when tab1 and tab2 are multiselected");
  BrowserTestUtils.removeTab(newTab);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  ok(!tab1.multiselected, "Tab1 is not multi-selected");
  ok(!tab2.multiselected, "Tab2 is not multi-selected");
  ok(!tab3.multiselected, "Tab3 is not multi-selected");

  promiseTabOpened = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(newTabButton, metaKeyEvent);
  openEvent = await promiseTabOpened;
  newTab = openEvent.target;
  is(newTab.previousElementSibling, tab1,
    "New tab should be opened after tab1 when only tab1 is selected");
  is(newTab.nextElementSibling, tab2,
    "New tab should be opened before tab2 when only tab1 is selected");
  BrowserTestUtils.removeTab(newTab);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab3, { ctrlKey: true });
  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(!tab2.multiselected, "Tab2 is not multi-selected");
  ok(tab3.multiselected, "Tab3 is multi-selected");

  promiseTabOpened = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(newTabButton, metaKeyEvent);
  openEvent = await promiseTabOpened;
  newTab = openEvent.target;
  is(newTab.previousElementSibling, tab3,
    "New tab should be opened after tab3 when tab1 and tab3 are selected");
  is(newTab.nextElementSibling, null,
    "New tab should be opened at the end of the tabstrip when tab1 and tab3 are selected");
  BrowserTestUtils.removeTab(newTab);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  ok(!tab1.multiselected, "Tab1 is not multi-selected");
  ok(!tab2.multiselected, "Tab2 is not multi-selected");
  ok(!tab3.multiselected, "Tab3 is not multi-selected");

  promiseTabOpened = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(newTabButton, {});
  openEvent = await promiseTabOpened;
  newTab = openEvent.target;
  is(newTab.previousElementSibling, tab3,
    "New tab should be opened after tab3 when ctrlKey is not used without multiselection");
  is(newTab.nextElementSibling, null,
    "New tab should be opened at the end of the tabstrip when ctrlKey is not used without multiselection");
  BrowserTestUtils.removeTab(newTab);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });
  ok(tab1.multiselected, "Tab1 is multi-selected");
  ok(tab2.multiselected, "Tab2 is multi-selected");
  ok(!tab3.multiselected, "Tab3 is not multi-selected");

  promiseTabOpened = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabOpen");
  EventUtils.synthesizeMouseAtCenter(newTabButton, {});
  openEvent = await promiseTabOpened;
  newTab = openEvent.target;
  is(newTab.previousElementSibling, tab3,
    "New tab should be opened after tab3 when ctrlKey is not used with multiselection");
  is(newTab.nextElementSibling, null,
    "New tab should be opened at the end of the tabstrip when ctrlKey is not used with multiselection");
  BrowserTestUtils.removeTab(newTab);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

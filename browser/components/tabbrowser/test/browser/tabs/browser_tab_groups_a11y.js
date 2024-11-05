"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.enabled", true]],
  });
});

add_task(async function test_TabGroupA11y() {
  const tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  const tabGroup = gBrowser.addTabGroup([tab2, tab3]);

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  Assert.equal(
    tabGroup.labelElement.getAttribute("role"),
    "button",
    "tab group label should have the button role"
  );

  Assert.equal(
    tabGroup.labelElement.getAttribute("aria-label"),
    "unnamed",
    "tab group label aria-label should default to 'unnamed' if not set"
  );

  Assert.equal(
    tabGroup.labelElement.getAttribute("aria-description"),
    "unnamed tab group",
    "tab group label aria-description should provide the name of the tab group plus more context"
  );

  Assert.equal(
    tabGroup.labelElement.getAttribute("aria-expanded"),
    "true",
    "tab group label aria-expanded should default to true, since tab groups default to not collapsed"
  );

  tabGroup.label = "test";
  tabGroup.collapsed = true;

  Assert.equal(
    tabGroup.labelElement.getAttribute("aria-label"),
    "test",
    "tab group label aria-label should equal the name of the tab group"
  );

  Assert.equal(
    tabGroup.labelElement.getAttribute("aria-description"),
    "test tab group",
    "tab group label aria-description should provide the name of the tab group plus more context"
  );

  Assert.equal(
    tabGroup.labelElement.getAttribute("aria-expanded"),
    "false",
    "tab group label aria-expanded should be false when the tab group is collapsed"
  );

  await removeTabGroup(tabGroup);
  BrowserTestUtils.removeTab(tab1);
});

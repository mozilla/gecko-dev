/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a web page with mixed active and display content
// on it while the "block mixed content" settings are _on_.
// It then checks that the mixed content flags have been set correctly.
// The test then overrides the MCB settings and checks that the flags
// have been set correctly again.
// Bug 838396 - Not setting hasMixedDisplayContentLoaded and
// hasMixedDisplayContentBlocked flag in nsMixedContentBlocker.cpp

const TEST_URI = "https://example.com/browser/browser/base/content/test/general/test-mixedcontent-securityerrors.html";
let gTestBrowser = null;

function test()
{
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({"set": [["security.mixed_content.block_active_content", true],
                            ["security.mixed_content.block_display_content", true]]}, blockMixedContentTest);
}

function blockMixedContentTest()
{
  gBrowser.selectedTab = gBrowser.addTab(TEST_URI);
  let tab = gBrowser.selectedTab;
  gTestBrowser = gBrowser.getBrowserForTab(tab);

  gTestBrowser.addEventListener("load", function onLoad(aEvent) {
    gTestBrowser.removeEventListener(aEvent.type, onLoad, true);
    is(gTestBrowser.docShell.hasMixedDisplayContentBlocked, true, "hasMixedDisplayContentBlocked flag has been set");
    is(gTestBrowser.docShell.hasMixedActiveContentBlocked, true, "hasMixedActiveContentBlocked flag has been set");
    is(gTestBrowser.docShell.hasMixedDisplayContentLoaded, false, "hasMixedDisplayContentLoaded flag has been set");
    is(gTestBrowser.docShell.hasMixedActiveContentLoaded, false, "hasMixedActiveContentLoaded flag has been set");
    overrideMCB();
  }, true);
}

function overrideMCB()
{
  // test mixed content flags on load (reload)
  gTestBrowser.addEventListener("load", mixedContentOverrideTest, true);
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger should appear");
  notification.reshow();
  ok(PopupNotifications.panel.firstChild.isMixedContentBlocked, "OK: Mixed Content is being blocked");

  // Make sure the notification has no mixedblockdisabled attribute
  ok(!PopupNotifications.panel.firstChild.hasAttribute("mixedblockdisabled"),
    "Doorhanger must have no mixedblockdisabled attribute");
  // Click on the doorhanger to allow mixed content (and reload page)
  PopupNotifications.panel.firstChild.disableMixedContentProtection();
  notification.remove();
}

function mixedContentOverrideTest()
{
  gTestBrowser.removeEventListener("load", mixedContentOverrideTest, true);

  is(gTestBrowser.docShell.hasMixedDisplayContentLoaded, true, "hasMixedDisplayContentLoaded flag has not been set");
  is(gTestBrowser.docShell.hasMixedActiveContentLoaded, true, "hasMixedActiveContentLoaded flag has not been set");
  is(gTestBrowser.docShell.hasMixedDisplayContentBlocked, false, "second hasMixedDisplayContentBlocked flag has been set");
  is(gTestBrowser.docShell.hasMixedActiveContentBlocked, false, "second hasMixedActiveContentBlocked flag has been set");

  let notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger should appear");
  notification.reshow();

  // Make sure the notification has the mixedblockdisabled attribute set to true
  is(PopupNotifications.panel.firstChild.getAttribute("mixedblockdisabled"), "true",
    "Doorhanger must have [mixedblockdisabled='true'] attribute");

  gBrowser.removeCurrentTab();
  finish();
}

/*
 * Description of the Tests for
 *  - Bug 902156: Persist "disable protection" option for Mixed Content Blocker
 *
 * 1. Navigate to the same domain via document.location
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a new page from the same origin using document.location
 *    - Doorhanger should not appear anymore!
 *
 * 2. Navigate to the same domain via simulateclick for a link on the page
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a new page from the same origin simulating a click
 *    - Doorhanger should not appear anymore!
 *
 * 3. Navigate to a differnet domain and show the content is still blocked
 *    - Load a different html page which has mixed content
 *    - Doorhanger to disable protection should appear again because
 *      we navigated away from html page where we disabled the protection.
 *
 * Note, for all tests we set gHttpTestRoot to use 'https'.
 */

const PREF_ACTIVE = "security.mixed_content.block_active_content";

// We alternate for even and odd test cases to simulate different hosts
const gHttpTestRoot1 = "https://test1.example.com/browser/browser/base/content/test/general/";
const gHttpTestRoot2 = "https://test2.example.com/browser/browser/base/content/test/general/";

var origBlockActive;
var gTestBrowser = null;

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
});

function cleanUpAfterTests() {
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

//------------------------ Test 1 ------------------------------

function test1A() {
  // Removing EventListener because we have to register a new
  // one once the page is loaded with mixed content blocker disabled
  gTestBrowser.removeEventListener("load", test1A, true);
  gTestBrowser.addEventListener("load", test1B, true);

  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  ok(notification, "OK: Mixed Content Doorhanger did appear in Test1A!");
  notification.reshow();
  ok(PopupNotifications.panel.firstChild.isMixedContentBlocked, "OK: Mixed Content is being blocked in Test1A!");

  // Disable Mixed Content Protection for the page (and reload)
  PopupNotifications.panel.firstChild.disableMixedContentProtection();
  notification.remove();
}

function test1B() {
  var expected = "Mixed Content Blocker disabled";
  waitForCondition(
    function() content.document.getElementById('mctestdiv').innerHTML == expected,
    test1C, "Error: Waited too long for mixed script to run in Test 1B");
}

function test1C() {
  gTestBrowser.removeEventListener("load", test1B, true);
  var actual = content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 1C");

  // The Script loaded after we disabled the page, now we are going to reload the
  // page and see if our decision is persistent
  gTestBrowser.addEventListener("load", test1D, true);

  var url = gHttpTestRoot1 + "file_bug902156_2.html";
  gTestBrowser.contentWindow.location = url;
}

function test1D() {
  gTestBrowser.removeEventListener("load", test1D, true);

  // The Doorhanger should appear but isMixedContentBlocked should be NOT true,
  // because our decision of disabling the mixed content blocker is persistent.
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  ok(notification, "OK: Mixed Content Doorhanger did appear in Test1D!");
  notification.reshow();
  ok(!PopupNotifications.panel.firstChild.isMixedContentBlocked, "OK: Mixed Content is NOT being blocked in Test1D!");
  notification.remove();

  var actual = content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 1D");

  // move on to Test 2
  test2();
}

//------------------------ Test 2 ------------------------------

function test2() {
  gTestBrowser.addEventListener("load", test2A, true);
  var url = gHttpTestRoot2 + "file_bug902156_2.html";
  gTestBrowser.contentWindow.location = url;
}

function test2A() {
  // Removing EventListener because we have to register a new
  // one once the page is loaded with mixed content blocker disabled
  gTestBrowser.removeEventListener("load", test2A, true);
  gTestBrowser.addEventListener("load", test2B, true);

  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  ok(notification, "OK: Mixed Content Doorhanger did appear in Test 2A!");
  notification.reshow();
  ok(PopupNotifications.panel.firstChild.isMixedContentBlocked, "OK: Mixed Content is being blocked in Test 2A!");

  // Disable Mixed Content Protection for the page (and reload)
  PopupNotifications.panel.firstChild.disableMixedContentProtection();
  notification.remove();
}

function test2B() {
  var expected = "Mixed Content Blocker disabled";
  waitForCondition(
    function() content.document.getElementById('mctestdiv').innerHTML == expected,
    test2C, "Error: Waited too long for mixed script to run in Test 2B");
}

function test2C() {
  gTestBrowser.removeEventListener("load", test2B, true);
  var actual = content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 2C");

  // The Script loaded after we disabled the page, now we are going to reload the
  // page and see if our decision is persistent
  gTestBrowser.addEventListener("load", test2D, true);

  // reload the page using the provided link in the html file
  var mctestlink = content.document.getElementById("mctestlink");
  mctestlink.click();
}

function test2D() {
  gTestBrowser.removeEventListener("load", test2D, true);

  // The Doorhanger should appear but isMixedContentBlocked should be NOT true,
  // because our decision of disabling the mixed content blocker is persistent.
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  ok(notification, "OK: Mixed Content Doorhanger did appear in Test2D!");
  notification.reshow();
  ok(!PopupNotifications.panel.firstChild.isMixedContentBlocked, "OK: Mixed Content is NOT being blocked");
  notification.remove();

  var actual = content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 2D");

  // move on to Test 3
  test3();
}

//------------------------ Test 3 ------------------------------

function test3() {
  gTestBrowser.addEventListener("load", test3A, true);
  var url = gHttpTestRoot1 + "file_bug902156_3.html";
  gTestBrowser.contentWindow.location = url;
}

function test3A() {
  // Removing EventListener because we have to register a new
  // one once the page is loaded with mixed content blocker disabled
  gTestBrowser.removeEventListener("load", test3A, true);

  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  ok(notification, "OK: Mixed Content Doorhanger did appear in Test 3A!");
  notification.reshow();
  ok(PopupNotifications.panel.firstChild.isMixedContentBlocked, "OK: Mixed Content is being blocked in Test 3A");
  notification.remove();

  // We are done with tests, clean up
  cleanUpAfterTests();
}

//------------------------------------------------------

function test() {
  // Performing async calls, e.g. 'onload', we have to wait till all of them finished
  waitForExplicitFinish();

  // Store original preferences so we can restore settings after testing
  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);

  Services.prefs.setBoolPref(PREF_ACTIVE, true);

  // Not really sure what this is doing
  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop()

  // Starting Test Number 1:
  gTestBrowser.addEventListener("load", test1A, true);
  var url = gHttpTestRoot1 + "file_bug902156_1.html";
  gTestBrowser.contentWindow.location = url;
}

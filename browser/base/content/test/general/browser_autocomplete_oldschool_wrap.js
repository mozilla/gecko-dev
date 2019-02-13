function repeat(limit, func) {
  for (let i = 0; i < limit; i++) {
    func(i);
  }
}

function is_selected(index) {
  is(gURLBar.popup.richlistbox.selectedIndex, index, `Item ${index + 1} should be selected`);
}

add_task(function*() {
  // This test is only relevant if UnifiedComplete is *disabled*.
  if (Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete")) {
    ok(true, "Don't run this test with UnifiedComplete enabled.")
    return;
  }

  yield PlacesTestUtils.clearHistory();
  let visits = [];
  repeat(10, i => {
    visits.push({
      uri: makeURI("http://example.com/autocomplete/?" + i),
    });
  });
  yield PlacesTestUtils.addVisits(visits);

  registerCleanupFunction(function* () {
    yield PlacesTestUtils.clearHistory();
  });

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla", {animate: false});
  yield promiseTabLoaded(tab);
  yield promiseAutocompleteResultPopup("example.com/autocomplete");

  let popup = gURLBar.popup;
  let results = popup.richlistbox.children.filter(is_visible);

  is(results.length, 10, "Should get 10 results");
  is_selected(-1);

  info("Key Down to select the next item");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is_selected(0);

  info("Key Up to select the previous item");
  EventUtils.synthesizeKey("VK_UP", {});
  is_selected(-1);

  info("Key Down to select the next item");
  EventUtils.synthesizeKey("VK_DOWN", {});
  is_selected(0);

  info("Key Down 11 times should wrap around all the way around");
  repeat(11, () => EventUtils.synthesizeKey("VK_DOWN", {}));
  is_selected(0);

  info("Key Up 11 times should wrap around the other way");
  repeat(11, () => EventUtils.synthesizeKey("VK_UP", {}));
  is_selected(0);

  info("Page Up will go up the list, but not wrap");
  repeat(4, () => EventUtils.synthesizeKey("VK_DOWN", {}));
  is_selected(4);
  EventUtils.synthesizeKey("VK_PAGE_UP", {})
  is_selected(0);

  info("Page Up again will wrap around to the end of the list");
  EventUtils.synthesizeKey("VK_PAGE_UP", {})
  is_selected(-1);

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});

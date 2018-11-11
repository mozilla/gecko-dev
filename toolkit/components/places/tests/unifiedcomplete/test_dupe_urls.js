/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure inline autocomplete doesn't return zero frecency pages.

add_task(function* test_dupe_urls() {
  do_print("Searching for urls with dupes should only show one");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/"),
    transition: TRANSITION_TYPED
  }, {
    uri: NetUtil.newURI("http://mozilla.org/?")
  });
  yield check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed:  "mozilla.org/",
    matches: [ { uri: NetUtil.newURI("http://mozilla.org/"),
                 title: "mozilla.org",
                 style: [ "autofill", "heuristic" ] } ]
  });
  yield cleanup();
});

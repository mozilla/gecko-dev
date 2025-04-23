/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function test_isSearchTab_returns_true_when_urls_match() {
  const tab = createMockTab({
    searchURL: "https://search.example.com/?q=hello",
    currentURL: "https://search.example.com/?q=hello",
  });
  ok(
    isSearchTab(tab),
    "Expected isSearchTab to return true when base URLs match"
  );
});

add_task(function test_isSearchTab_returns_true_when_query_changes() {
  const tab = createMockTab({
    searchURL: "https://search.example.com/?q=hello",
    currentURL: "https://search.example.com/?q=world",
  });
  ok(
    isSearchTab(tab),
    "Expected isSearchTab to return true when search query changes"
  );
});

add_task(function test_isSearchTab_returns_false_when_no_linked_browser() {
  const tab = { linkedBrowser: null };
  ok(
    !isSearchTab(tab),
    "Expected isSearchTab to return false when there is no browser info"
  );
});

add_task(function test_isSearchTab_returns_false_when_no_search_engine() {
  const tab = createMockTab({
    currentURL: "https://search.example.com/?q=world",
  });
  ok(
    !isSearchTab(tab),
    "Expected isSearchTab to return false when there is no search query"
  );
});

add_task(function test_isSearchTab_returns_false_when_no_search_url() {
  const tab = createMockTab({
    searchURL: null,
    currentURL: "https://search.example.com/?q=something",
  });
  ok(
    !isSearchTab(tab),
    "Expected isSearchTab to return false when searchURL is null"
  );
});

add_task(function test_isSearchTab_returns_false_when_user_browses_away() {
  const tab = createMockTab({
    searchURL: "https://search.example.com/?q=hello",
    currentURL: "https://cnn.com/",
  });
  ok(
    !isSearchTab(tab),
    "Expected isSearchTab to return false when base URLs don't match"
  );
});

add_task(function test_isSearchTab_returns_false_when_query_marker_missing() {
  const tab = createMockTab({
    searchURL: "https://search.example.com/search",
    currentURL: "https://search.example.com/search?q=test",
  });
  ok(
    !isSearchTab(tab),
    "Expected isSearchTab to return false when searchURL has no query marker"
  );
});

function createClusterWithTab(tabTitle) {
  const tab = createMockTab({ title: tabTitle });
  const config = SMART_TAB_GROUPING_CONFIG;
  return new ClusterRepresentation({
    tabs: [tab],
    embeddings: [[]],
    centroid: [],
    config,
  });
}

add_task(function test_setSingleTabSearchLabel_sets_label_correctly() {
  const cluster = createClusterWithTab("Weather Forecast - Weather.com");

  const success = cluster.setSingleTabSearchLabel();
  ok(success, "Expected setSingleTabSearchLabel to succeed");
  is(
    cluster.predictedTopicLabel,
    "Weather Forecast",
    "Expected predicted topic label to match before delimiter and be title-cased"
  );
});

add_task(function test_setSingleTabSearchLabel_too_long_no_update() {
  const cluster = createClusterWithTab(
    "This is a very long title that should be summarized | searchfox"
  );

  const success = cluster.setSingleTabSearchLabel();
  ok(
    !success,
    "Expected setSingleTabSearchLabel to fail due to long topic string"
  );
  is(
    cluster.predictedTopicLabel,
    null,
    "Expected predicted topic label to remain null"
  );
});

add_task(function test_setSingleTabSearchLabel_no_delimiter() {
  const cluster = createClusterWithTab("No Delimiter Here");

  const success = cluster.setSingleTabSearchLabel();
  ok(
    !success,
    "Expected setSingleTabSearchLabel to fail due to missing delimiter"
  );
  is(
    cluster.predictedTopicLabel,
    null,
    "Expected predicted topic label to remain null"
  );
});

add_task(function test_setSingleTabSearchLabel_multiple_delimiters() {
  const cluster = createClusterWithTab("AI tools - GPT Models | Search");

  const success = cluster.setSingleTabSearchLabel();
  ok(
    success,
    "Expected setSingleTabSearchLabel to succeed with multiple delimeters and capitalize first letters"
  );
  is(
    cluster.predictedTopicLabel,
    "AI Tools - GPT Models",
    "Expected label to cut off at first delimiter"
  );
});

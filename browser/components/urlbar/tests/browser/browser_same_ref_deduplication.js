/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const RESULT_SOURCE = UrlbarUtils.RESULT_SOURCE;

const now = new Date();
const oneWeekAgo = new Date();
const twoWeeksAgo = new Date();

add_setup(async function () {
  oneWeekAgo.setDate(oneWeekAgo.getDate() - 7);
  twoWeeksAgo.setDate(twoWeeksAgo.getDate() - 14);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.deduplication.enabled", true],
      ["browser.urlbar.deduplication.thresholdDays", 1],
    ],
  });
});

add_task(async function test_only_old_visits() {
  let urlBase = "https://example.com/page1";
  let title = "Page 1";
  await PlacesTestUtils.addVisits([
    { url: urlBase, title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref1", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref2", title, visitDate: oneWeekAgo },
  ]);

  info("Searching by URL");
  await urlbarQuery(urlBase);
  Assert.equal(
    await numHistoryResults(),
    1,
    "Only one result stays after deduplication"
  );

  info("Searching by title");
  await urlbarQuery(title);
  Assert.equal(
    await numHistoryResults(),
    1,
    "Only one result stays after deduplication"
  );
  await PlacesUtils.history.clear();
});

add_task(async function test_only_recent_visits() {
  let urlBase = "https://example.com/page1";
  let title = "Page 1";
  await PlacesTestUtils.addVisits([
    { url: urlBase, title, visitDate: now },
    { url: urlBase + "#ref1", title, visitDate: now },
    { url: urlBase + "#ref2", title, visitDate: now },
  ]);

  info("Searching by URL");
  await urlbarQuery(urlBase);
  Assert.equal(await numHistoryResults(), 3, "No results got deduplicated");

  info("Searching by title");
  await urlbarQuery(title);
  Assert.equal(await numHistoryResults(), 3, "No results got deduplicated");
  await PlacesUtils.history.clear();
});

add_task(async function test_not_deduplicate_top_frecency() {
  let urlBase = "https://example.com/page1";
  let title = "Page 1";
  await PlacesTestUtils.addVisits([
    { url: urlBase, title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref1", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref1", title, visitDate: twoWeeksAgo },
    { url: urlBase + "#ref2", title, visitDate: oneWeekAgo },
  ]);

  await urlbarQuery(title);
  let historyResults = await getResultsHavingSource(RESULT_SOURCE.HISTORY);
  Assert.equal(
    historyResults.length,
    1,
    "Only one result stays after deduplication"
  );
  Assert.equal(
    historyResults[0].url,
    "https://example.com/page1#ref1",
    "The URL with the highest frecency didn't get deduplicated"
  );
  await PlacesUtils.history.clear();
});

add_task(async function test_not_deduplicate_top_frecency_and_recent() {
  let urlBase = "https://example.com/page1";
  let title = "Page 1";
  await PlacesTestUtils.addVisits([
    // Recent visit should not get deduplicated.
    { url: urlBase, title, visitDate: now },
    // History entry with highest frecency should not get deduplicated.
    { url: urlBase + "#ref1", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref1", title, visitDate: twoWeeksAgo },
    { url: urlBase + "#ref2", title, visitDate: oneWeekAgo },
  ]);

  await urlbarQuery(title);
  let historyResults = await getResultsHavingSource(RESULT_SOURCE.HISTORY);
  Assert.equal(historyResults.length, 2, "Only one result got deduplicated");
  Assert.equal(
    historyResults[0].url,
    "https://example.com/page1#ref1",
    "The one with the highest frecency is first and didn't get deduplicated"
  );
  Assert.equal(
    historyResults[1].url,
    "https://example.com/page1",
    "The one with the recent visit didn't get deduplicated"
  );
  await PlacesUtils.history.clear();
});

add_task(async function test_count_tabs_as_duplicates() {
  // We use this because visiting a non existent page would mess up the title.
  let urlBase =
    "https://example.com/browser/browser/base/content/test/general/dummy_page.html";
  let title = "Dummy test page";
  await PlacesTestUtils.addVisits([
    { url: urlBase, title, visitDate: oneWeekAgo },
    // This one will have the highest frecency and be open in another tab.
    { url: urlBase + "#ref1", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref1", title, visitDate: twoWeeksAgo },
    { url: urlBase + "#ref2", title, visitDate: oneWeekAgo },
  ]);

  await loadUrl(urlBase + "#ref1");
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await urlbarQuery(title);
  let historyResults = await getResultsHavingSource(RESULT_SOURCE.HISTORY);
  let tabResults = await getResultsHavingSource(RESULT_SOURCE.TABS);
  Assert.equal(
    historyResults.length,
    0,
    "Sites with lower frecency got deduplicated"
  );
  Assert.equal(tabResults.length, 1, "One tab result");
  Assert.equal(
    tabResults[0].url,
    urlBase + "#ref1",
    "The tab result is the site with the highest frecency"
  );

  BrowserTestUtils.removeTab(newTab);
  await loadUrl("about:blank");
  await PlacesUtils.history.clear();
});

add_task(async function test_count_bookmarks_as_duplicates() {
  let urlBase = "https://example.com/page1";
  let title = "Dummy test page";
  await PlacesTestUtils.addVisits([
    { url: urlBase, title, visitDate: oneWeekAgo },
    // This one will have the highest frecency and be bookmarked.
    { url: urlBase + "#ref1", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref1", title, visitDate: twoWeeksAgo },
    { url: urlBase + "#ref2", title, visitDate: oneWeekAgo },
  ]);

  let bookmarkGuid = await PlacesUtils.bookmarks.insert({
    url: urlBase + "#ref1",
    title,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  await urlbarQuery(title);
  let historyResults = await getResultsHavingSource(RESULT_SOURCE.HISTORY);
  let bookmarkResults = await getResultsHavingSource(RESULT_SOURCE.BOOKMARKS);
  Assert.equal(
    historyResults.length,
    0,
    "Sites with lower frecency got deduplicated"
  );
  Assert.equal(bookmarkResults.length, 1, "One bookmark result");
  Assert.equal(
    bookmarkResults[0].url,
    urlBase + "#ref1",
    "The bookmark result is the site with the highest frecency"
  );

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.remove(bookmarkGuid);
});

add_task(async function test_not_deduplicate_different_titles() {
  let urlBase = "https://example.com/page";
  await PlacesTestUtils.addVisits([
    { url: urlBase + "#ref1", title: "Page 1", visitDate: oneWeekAgo },
    { url: urlBase + "#ref2", title: "Page 1", visitDate: oneWeekAgo },
    { url: urlBase + "#ref3", title: "Page 2", visitDate: oneWeekAgo },
    { url: urlBase + "#ref4", title: "Page 2", visitDate: oneWeekAgo },
  ]);

  info("Searching by URL");
  await urlbarQuery(urlBase);
  Assert.equal(
    await numHistoryResults(),
    2,
    "Got one result for each of the two titles"
  );

  info("Searching by title");
  await urlbarQuery("Page ");
  Assert.equal(
    await numHistoryResults(),
    2,
    "Got one result for each of the two titles"
  );
  await PlacesUtils.history.clear();
});

add_task(async function test_with_input_history() {
  let urlBase = "https://example.com/page";
  let title = "Page 1";
  await PlacesTestUtils.addVisits([
    // ref1 will have the highest frecency.
    { url: urlBase + "#ref1", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref1", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref2", title, visitDate: oneWeekAgo },
    { url: urlBase + "#ref3", title, visitDate: oneWeekAgo },
  ]);
  let input = "Pag";
  await UrlbarUtils.addToInputHistory(urlBase + "#ref2", input);

  for (let i = 1; i < 4; i++) {
    let query = input.substring(0, i);
    info(`Searching for title ${query}`);
    await urlbarQuery(query);

    let historyResults = await getResultsHavingSource(RESULT_SOURCE.HISTORY);
    Assert.equal(historyResults.length, 1, "Only one history result");
    Assert.equal(
      historyResults[0].url,
      urlBase + "#ref2",
      "Only the result from the input history."
    );
  }

  info("Searching for title Page");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "Page",
  });

  let historyResults = await getResultsHavingSource(RESULT_SOURCE.HISTORY);
  Assert.equal(historyResults.length, 1, "Only one history result");
  Assert.equal(
    historyResults[0].url,
    urlBase + "#ref1",
    "Only the result with the highest frecency."
  );
  await PlacesUtils.history.clear();
});

add_task(async function test_last_visit_property() {
  // We use this because visiting a non existent page would mess up the title.
  let urlBase =
    "https://example.com/browser/browser/base/content/test/general/dummy_page.html";
  let title = "Dummy test page";
  await PlacesTestUtils.addVisits([
    { url: urlBase, title, visitDate: now },
    { url: urlBase + "#ref1", title, visitDate: now },
    { url: urlBase + "#ref2", title, visitDate: now },
  ]);
  await UrlbarUtils.addToInputHistory(urlBase + "#ref2", "Dummy");

  await loadUrl(urlBase + "#ref1");
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  await urlbarQuery("Dummy");
  let tabResults = await getResultsHavingSource(RESULT_SOURCE.TABS);
  let historyResults = await getResultsHavingSource(RESULT_SOURCE.HISTORY);

  Assert.equal(tabResults.length, 1, "One tab result");
  Assert.equal(historyResults.length, 2, "Two history results");
  console.log(historyResults.map(r => r.result.providerName));
  Assert.ok(
    historyResults[0].result.providerName == "InputHistory",
    "First history result is from InputHistoryProvider"
  );
  Assert.ok(
    tabResults.every(e => e.result.payload.lastVisit >= now.getTime()),
    "The tab result has the correct lastVisit"
  );
  Assert.ok(
    historyResults.every(e => e.result.payload.lastVisit == now.getTime()),
    "All history results have the correct lastVisit"
  );

  BrowserTestUtils.removeTab(newTab);
  await loadUrl("about:blank");
  await PlacesUtils.history.clear();
});

add_task(async function test_extractRef() {
  let { base, ref } = UrlbarUtils.extractRefFromUrl(
    "https://example.com/page1#ref1"
  );
  Assert.equal(base, "https://example.com/page1", "Base is correct");
  Assert.equal(ref, "ref1", "Ref is correct");
});

add_task(async function test_extractEmptyRef() {
  let { base, ref } = UrlbarUtils.extractRefFromUrl(
    "https://example.com/page1"
  );
  Assert.equal(base, "https://example.com/page1", "Base is correct");
  Assert.equal(ref, "", "Ref is correct");
});

async function numHistoryResults() {
  return (await getResultsHavingSource(RESULT_SOURCE.HISTORY)).length;
}

async function getResultsHavingSource(source) {
  let count = UrlbarTestUtils.getResultCount(window);
  let results = [];
  for (let i = 0; i < count; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (result.source == source) {
      results.push(result);
    }
  }
  return results;
}

async function loadUrl(url) {
  let promiseLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    url
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, url);
  await promiseLoaded;
}

async function urlbarQuery(query) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: query,
  });
}

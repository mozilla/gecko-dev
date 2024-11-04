/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONFIG = [
  {
    identifier: "engine_searchform",
    base: {
      urls: {
        searchForm: {
          base: "https://example.com/searchform",
          params: [{ name: "foo", value: "bar" }],
        },
      },
    },
  },
  {
    identifier: "engine_no_searchform",
    base: {
      urls: {
        search: {
          base: "https://example.com/search",
          searchTermParamName: "q",
        },
      },
    },
  },
];

add_setup(async function () {
  useHttpServer("");

  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.init();

  await SearchTestUtils.installSearchExtension({
    name: "AddonEngine",
    search_url: "https://example.com/search",
    search_url_get_params: "q={searchTerms}",
  });
});

add_task(async function test_appProvidedEngineSearchform() {
  let engine = Services.search.getEngineById(`engine_searchform`);
  Assert.equal(
    engine.searchForm,
    "https://example.com/searchform?foo=bar",
    "Used specified searchForm with parameters."
  );
});

add_task(async function test_appProvidedEngineNoSearchform() {
  let engine = Services.search.getEngineById(`engine_no_searchform`);
  Assert.equal(
    engine.searchForm,
    "https://example.com",
    "Used pre path of search URL."
  );
});

add_task(async function test_addonEngine() {
  let engine2 = Services.search.getEngineByName(`AddonEngine`);
  Assert.equal(
    engine2.searchForm,
    "https://example.com",
    "Used pre path of search URL."
  );
});

add_task(async function test_openSearchRel() {
  // This engine has its searchForm defined as a Url tag with rel="searchform" attribute.
  let engine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/opensearch/suggestion.xml`,
  });
  Assert.equal(
    engine.searchForm,
    "http://engine-rel-searchform.xml/?search&some=param",
    "Used specified searchForm with parameters."
  );
});

add_task(async function test_openSearchElement() {
  // This engine has its searchForm defined as a <SearchForm> element.
  let engine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/opensearch/images.xml`,
  });
  Assert.equal(
    engine.searchForm,
    "http://www.bing.com/search",
    "Used specified searchForm."
  );
});

add_task(async function test_openSearchNoSearchform() {
  // This engine has no custom searchForm.
  let engine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/opensearch/simple.xml`,
  });
  Assert.equal(
    engine.searchForm,
    "https://example.com",
    "Used pre path of search URL."
  );
});

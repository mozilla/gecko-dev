/*
 * This test checks we can determine the type of search page and whether the
 * page is a default search page.
 */

ChromeUtils.defineESModuleGetters(this, {
  UrlbarSearchTermsPersistence:
    "resource:///modules/UrlbarSearchTermsPersistence.sys.mjs",
});

const PROVIDERS = [
  {
    id: "example",
    queryParamNames: ["q"],
    searchPageRegexp: "^https://example\\.com/",
    includeParams: [
      {
        key: "page",
        values: ["web"],
        canBeMissing: true,
      },
    ],
    excludeParams: [
      {
        key: "excludeKey",
        values: ["hello"],
      },
    ],
  },
  {
    id: "example2",
    queryParamNames: ["q"],
    searchPageRegexp: "^https://example2\\.com/",
    includeParams: [
      {
        key: "page",
        values: ["web"],
        canBeMissing: false,
      },
    ],
    excludeParams: [
      {
        key: "excludeKey",
      },
    ],
  },
];

const TESTS = [
  {
    title: "Tracked provider",
    name: "Example",
    cases: [
      {
        title: "Non-existent URI",
        currentURI: null,
        originalURI: null,
        expected: "",
      },
      {
        title: "Empty URI",
        currentURI: "",
        originalURI: "",
        expected: "",
      },
      {
        title: "Non-url",
        currentURI: "about:blank",
        originalURI: "about:blank",
        expected: "",
      },
      {
        title: "Non-url 1",
        currentURI: "about:home",
        originalURI: "about:home",
        expected: "",
      },
      {
        title: "Non-url 2",
        currentURI: "about:newtab",
        originalURI: "about:newtab",
        expected: "",
      },
      {
        title: "Non-url 3",
        currentURI: "not://a/supported/protocol",
        originalURI: "not://a/supported/protocol",
        expected: "",
      },
      {
        title: "Non-url 4",
        currentURI: "view-source:http://www.example.com/",
        originalURI: "view-source:http://www.example.com/",
        expected: "",
      },
      {
        title: "No search parameters",
        currentURI: "https://example.com/",
        originalURI: "https://example.com/",
        expected: "",
      },
      {
        title: "No search query param value",
        currentURI: "https://example.com/?q",
        originalURI: "https://example.com/?q",
        expected: "",
      },
      {
        title: "With search query param value",
        currentURI: "https://example.com/?q=foo",
        originalURI: "https://example.com/?q=foo",
        expected: "foo",
      },
      {
        title: "With search query param value and a fake value key-value pair",
        currentURI: "https://example.com/?q=foo&page=fake_code",
        originalURI: "https://example.com/?q=foo&page=fake_code",
        expected: "",
      },
      {
        title: "With search query param value and valid key-value pair",
        currentURI: "https://example.com/?q=foo&page=web",
        originalURI: "https://example.com/?q=foo&page=web",
        expected: "foo",
      },
      {
        title: "With search query param value and unknown key-value pair",
        currentURI: "https://example.com/?q=foo&key=unknown",
        originalURI: "https://example.com/?q=foo&key=unknown",
        expected: "foo",
      },
      {
        title:
          "With search query param value and valid excludeParams key-value pair",
        currentURI: "https://example.com/?q=foo&excludeKey=hello",
        originalURI: "https://example.com/?q=foo&excludeKey=hello",
        expected: "",
      },
      {
        title:
          "With search query param value and invalid excludeParams key-value pair",
        currentURI: "https://example.com/?q=foo&page=web&excludeKey=hi",
        originalURI: "https://example.com/?q=foo&page=web&excludeKey=hi",
        expected: "foo",
      },
    ],
  },
  {
    title: "Untracked provider",
    name: "Example1",
    cases: [
      {
        title: "With search query param",
        currentURI: "https://example1.com/?q=foo",
        originalURI: "https://example1.com/?q=foo",
        expected: "",
      },
      {
        title: "No search parameters",
        currentURI: "https://example1.com/",
        originalURI: "https://example1.com/",
        expected: "",
      },
      {
        title: "No search query param value",
        currentURI: "https://example1.com/?q",
        originalURI: "https://example1.com/?q",
        expected: "",
      },
      {
        title: "With search query param value and unknown key-value pair",
        currentURI: "https://example1.com/?q=foo&key=unknown",
        originalURI: "https://example1.com/?q=foo&key=unknown",
        expected: "",
      },
      {
        title: "with search query param value and default key-value pair",
        currentURI: "https://example1.com/?q=foo&default=all",
        originalURI: "https://example1.com/?q=foo&default=all",
        expected: "foo",
      },
    ],
  },
  {
    title: "Tracked provider with mandatory key-value pair",
    name: "Example2",
    cases: [
      {
        title: "With search query param value",
        currentURI: "https://example2.com/?q=foo",
        originalURI: "https://example2.com/?q=foo",
        expected: "",
      },
      {
        title: "With search query param value",
        currentURI: "https://example2.com/?q=foo&page=web",
        originalURI: "https://example2.com/?q=foo&page=web",
        expected: "foo",
      },
      {
        title: "With search query param value and excludeParams key-value pair",
        currentURI: "https://example2.com/?q=foo&page=web&excludeKey=web",
        originalURI: "https://example2.com/?q=foo&page=web&excludeKey=web",
        expected: "",
      },
      {
        title:
          "With search query param value and presence of excludeParams key",
        currentURI: "https://example2.com/?q=foo&excludeKey",
        originalURI: "https://example2.com/?q=foo&excludeKey",
        expected: "",
      },
    ],
  },
];

add_setup(async function () {
  await UrlbarSearchTermsPersistence.overrideSearchTermsPersistenceForTests(
    PROVIDERS
  );
  const CONFIG_V2 = [
    {
      recordType: "engine",
      identifier: "Example",
      base: {
        name: "Example",
        urls: {
          search: {
            base: "https://example.com/",
            searchTermParamName: "q",
          },
        },
      },
    },
    {
      recordType: "engine",
      identifier: "Example1",
      base: {
        name: "Example1",
        urls: {
          search: {
            base: "https://example1.com/",
            searchTermParamName: "q",
            params: [
              {
                name: "default",
                value: "all",
              },
            ],
          },
        },
      },
    },
    {
      recordType: "engine",
      identifier: "Example2",
      base: {
        name: "Example2",
        urls: {
          search: {
            base: "https://example2.com/",
            searchTermParamName: "q",
          },
        },
      },
    },
    {
      recordType: "defaultEngines",
      globalDefault: "Example",
      specificDefaults: [],
    },
  ];
  SearchTestUtils.updateRemoteSettingsConfig(CONFIG_V2);
  await Services.search.init();
});

add_task(async function test_parsing_extracted_urls() {
  for (let test of TESTS) {
    for (let { title, currentURI, originalURI, expected } of test.cases) {
      info(`${test.name} - ${title}`);

      if (currentURI) {
        currentURI = Services.io.newURI(currentURI);
      }
      if (originalURI) {
        originalURI = Services.io.newURI(originalURI);
      }

      Assert.equal(
        UrlbarSearchTermsPersistence.getSearchTerm(originalURI, currentURI),
        expected
      );
    }
  }
});

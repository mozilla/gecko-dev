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
        originalURI: null,
        expected: "",
      },
      {
        title: "Empty URI",
        originalURI: "",
        expected: "",
      },
      {
        title: "Non-url",
        originalURI: "about:blank",
        expected: "",
      },
      {
        title: "Non-url 1",
        originalURI: "about:home",
        expected: "",
      },
      {
        title: "Non-url 2",
        originalURI: "about:newtab",
        expected: "",
      },
      {
        title: "Non-url 3",
        originalURI: "not://a/supported/protocol",
        expected: "",
      },
      {
        title: "Non-url 4",
        originalURI: "view-source:http://www.example.com/",
        expected: "",
      },
      {
        title: "No search parameters",
        originalURI: "https://example.com/",
        expected: "",
      },
      {
        title: "No search query param value",
        originalURI: "https://example.com/?q",
        expected: "",
      },
      {
        title: "With search query param value",
        originalURI: "https://example.com/?q=foo",
        expected: "foo",
      },
      {
        title: "With search query param value and a fake value key-value pair",
        originalURI: "https://example.com/?q=foo&page=fake_code",
        expected: "",
      },
      {
        title: "With search query param value and valid key-value pair",
        originalURI: "https://example.com/?q=foo&page=web",
        expected: "foo",
      },
      {
        title: "With search query param value and unknown key-value pair",
        originalURI: "https://example.com/?q=foo&key=unknown",
        expected: "foo",
      },
      {
        title:
          "With search query param value and valid excludeParams key-value pair",
        originalURI: "https://example.com/?q=foo&excludeKey=hello",
        expected: "",
      },
      {
        title:
          "With search query param value and invalid excludeParams key-value pair",
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
        originalURI: "https://example1.com/?q=foo",
        expected: "",
      },
      {
        title: "No search parameters",
        originalURI: "https://example1.com/",
        expected: "",
      },
      {
        title: "No search query param value",
        originalURI: "https://example1.com/?q",
        expected: "",
      },
      {
        title: "With search query param value and unknown key-value pair",
        originalURI: "https://example1.com/?q=foo&key=unknown",
        expected: "",
      },
      {
        title: "with search query param value and default key-value pair",
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
        originalURI: "https://example2.com/?q=foo",
        expected: "",
      },
      {
        title: "With search query param value",
        originalURI: "https://example2.com/?q=foo&page=web",
        expected: "foo",
      },
      {
        title: "With search query param value and excludeParams key-value pair",
        originalURI: "https://example2.com/?q=foo&page=web&excludeKey=web",
        expected: "",
      },
      {
        title:
          "With search query param value and presence of excludeParams key",
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
    for (let { title, originalURI, expected } of test.cases) {
      info(`${test.name} - ${title}`);

      if (originalURI) {
        originalURI = Services.io.newURI(originalURI);
      }

      Assert.equal(
        UrlbarSearchTermsPersistence.getSearchTerm(originalURI),
        expected
      );
    }
  }
});

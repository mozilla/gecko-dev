/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that a search purpose can be specified and that query parameters for
 * that purpose are included in the search URL.
 */

"use strict";

const CONFIG = [
  {
    identifier: "engineWithPurposes",
    base: {
      urls: {
        search: {
          base: "https://www.example.com/search",
          params: [
            {
              name: "channel",
              searchAccessPoint: {
                addressbar: "fflb",
                contextmenu: "rcs",
              },
            },
            {
              name: "extra",
              value: "foo",
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
  },
  {
    identifier: "engineWithPurposesReordered",
    base: {
      urls: {
        search: {
          base: "https://www.example.com/search",
          params: [
            // This time we put the purposes second, to ensure they are
            // correctly inserted regardless of where they are in the URL.
            {
              name: "extra",
              value: "foo",
            },
            {
              name: "channel",
              searchAccessPoint: {
                addressbar: "fflb",
                contextmenu: "rcs",
              },
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
  },
];

add_setup(async function () {
  // The test engines used in this test need to be recognized as 'default'
  // engines, or their MozParams used to set the purpose will be ignored.
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.init();
});

add_task(async function test_purpose() {
  let engines = [
    Services.search.getEngineById("engineWithPurposes"),
    Services.search.getEngineById("engineWithPurposesReordered"),
  ];

  function check_submission(engine, value, searchTerm, type, purpose) {
    let submissionURL = engine.getSubmission(searchTerm, type, purpose).uri
      .spec;
    let searchParams = new URLSearchParams(submissionURL.split("?")[1]);
    if (value) {
      Assert.equal(searchParams.get("channel"), value);
    } else {
      Assert.ok(!searchParams.has("channel"));
    }
    Assert.equal(searchParams.get("q"), searchTerm);
  }

  for (let engine of engines) {
    info(`Testing ${engine.identifier}`);
    check_submission(engine, "", "foo");
    check_submission(engine, "", "foo", "text/html");
    check_submission(engine, "", "foo", null);
    check_submission(engine, "rcs", "foo", null, "contextmenu");
    check_submission(engine, "rcs", "foo", "text/html", "contextmenu");
    check_submission(engine, "fflb", "foo", null, "keyword");
    check_submission(engine, "fflb", "foo", "text/html", "keyword");
    check_submission(engine, "", "foo", "text/html", "invalid");
  }
});

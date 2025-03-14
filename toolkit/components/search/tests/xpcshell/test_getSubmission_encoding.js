/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const prefix = "https://www.example.com/search";

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig([
    {
      identifier: "utf8_param",
      base: {
        charset: "UTF-8",
        urls: {
          search: {
            base: "https://www.example.com/search",
            searchTermParamName: "q",
          },
        },
      },
    },
    {
      identifier: "utf8_url",
      base: {
        charset: "UTF-8",
        urls: {
          search: {
            base: "https://www.example.com/search/{searchTerms}",
          },
        },
      },
    },
    { identifier: "windows1252", base: { charset: "windows-1252" } },
  ]);
  await Services.search.init();
});

function testEncode(engine, charset, query, expected) {
  Assert.equal(
    engine.getSubmission(query).uri.spec,
    prefix + expected,
    `Should have correctly encoded for ${charset}`
  );
}

add_task(async function test_getSubmission_utf8_param() {
  let engine = Services.search.getEngineById("utf8_param");
  // Space should be encoded to + since the search terms are a parameter.
  testEncode(engine, "UTF-8", "caff\u00E8 shop +", "?q=caff%C3%A8+shop+%2B");
});

add_task(async function test_getSubmission_utf8_url() {
  let engine = Services.search.getEngineById("utf8_url");
  // Space should be encoded to %20 since the search terms are part of the URL.
  testEncode(engine, "UTF-8", "caff\u00E8 shop +", "/caff%C3%A8%20shop%20%2B");
});

add_task(async function test_getSubmission_windows1252() {
  let engine = Services.search.getEngineById("windows1252");
  testEncode(engine, "windows-1252", "caff\u00E8+", "?q=caff%E8%2B");
});

// Spaces are percent-encoded to either + or %20, depending on the url component.
add_task(async function test_encoding_of_spaces() {
  info("Testing spaces in query.");
  let engine = await Services.search.addUserEngine({
    name: "user",
    url: "https://example.com/user?q={searchTerms}#ref",
  });
  Assert.equal(
    engine.getSubmission("f o o").uri.spec,
    "https://example.com/user?q=f+o+o#ref",
    "Encodes spaces in query as +."
  );
  await Services.search.removeEngine(engine);

  info("Testing spaces in path.");
  engine = await Services.search.addUserEngine({
    name: "user",
    url: "https://example.com/user/{searchTerms}?que=ry#ref",
  });
  Assert.equal(
    engine.getSubmission("f o o").uri.spec,
    "https://example.com/user/f%20o%20o?que=ry#ref",
    "Encodes spaces in path as %20."
  );
  await Services.search.removeEngine(engine);

  info("Testing spaces in ref.");
  engine = await Services.search.addUserEngine({
    name: "user",
    url: "https://example.com/user?que=ry#{searchTerms}",
  });
  Assert.equal(
    engine.getSubmission("f o o").uri.spec,
    "https://example.com/user?que=ry#f%20o%20o",
    "Encodes spaces in ref as %20."
  );
  await Services.search.removeEngine(engine);

  info("Testing spaces in post data.");
  let formData = new FormData();
  formData.append("q", "{searchTerms}");
  engine = await Services.search.addUserEngine({
    name: "user",
    url: "https://example.com/user",
    formData,
    method: "POST",
  });
  let submission = engine.getSubmission("f o o");
  Assert.equal(submission.uri.spec, "https://example.com/user");
  Assert.equal(
    postDataToString(submission.postData),
    "q=f+o+o",
    "Encodes spaces in body as +."
  );
  await Services.search.removeEngine(engine);
});

function postDataToString(postData) {
  if (!postData) {
    return undefined;
  }
  let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  binaryStream.setInputStream(postData.data);

  return binaryStream
    .readBytes(binaryStream.available())
    .replace("searchTerms", "%s");
}

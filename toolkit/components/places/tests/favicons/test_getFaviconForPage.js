/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for getFaviconForPage()
 */

add_task(async function test_basic() {
  const favicon = await createFavicon("favicon.png");

  const pageURI = uri("http://example.com/");
  const faviconURI = uri("http://example.com/favicon.png");
  const faviconDataURI = await createDataURLForFavicon(favicon);

  await doTestGetFaviconForPage({
    pageURI,
    faviconURI,
    faviconDataURI,
    expectedFavicon: {
      uri: faviconURI,
      dataURI: faviconDataURI,
      rawData: favicon.data,
      mimeType: "image/png",
      width: 16,
    },
  });

  await IOUtils.remove(favicon.file.path);
});

add_task(async function test_svg() {
  const favicon = "<svg><rect width='1px' height='1px'/></svg>";

  const pageURI = uri("http://example.com/");
  const faviconURI = uri("http://example.com/favicon.svg");
  const faviconDataURI = uri(`data:image/svg+xml;utf8,${favicon}`);

  await doTestGetFaviconForPage({
    pageURI,
    faviconURI,
    faviconDataURI,
    expectedFavicon: {
      uri: faviconURI,
      dataURI: uri(`data:image/svg+xml;base64,${base64EncodeString(favicon)}`),
      rawData: Array.from(favicon).map(s => s.charCodeAt(0)),
      mimeType: "image/svg+xml",
      width: 65535,
    },
  });
});

add_task(async function test_userpass() {
  const favicon = await createFavicon("favicon.png");

  const pageURI = uri("http://user:pass@example.com/");
  const faviconURI = uri("http://user:pass@example.com/favicon.png");
  const faviconDataURI = await createDataURLForFavicon(favicon);

  await doTestGetFaviconForPage({
    pageURI,
    faviconURI,
    faviconDataURI,
    expectedFavicon: {
      uri: uri("http://example.com/favicon.png"),
      dataURI: faviconDataURI,
      rawData: favicon.data,
      mimeType: "image/png",
      width: 16,
    },
  });

  await IOUtils.remove(favicon.file.path);
});

async function doTestGetFaviconForPage({
  pageURI,
  faviconURI,
  faviconDataURI,
  expectedFavicon,
}) {
  await PlacesTestUtils.addVisits(pageURI);
  await PlacesUtils.favicons.setFaviconForPage(
    pageURI,
    faviconURI,
    faviconDataURI
  );

  const result = await PlacesUtils.favicons.getFaviconForPage(pageURI);
  Assert.equal(result.uri.spec, expectedFavicon.uri.spec);
  Assert.equal(result.dataURI.spec, expectedFavicon.dataURI.spec);
  Assert.equal(result.rawData.join(","), expectedFavicon.rawData.join(","));
  Assert.equal(result.mimeType, expectedFavicon.mimeType);
  Assert.equal(result.width, expectedFavicon.width);

  await PlacesUtils.history.clear();
  await PlacesTestUtils.clearFavicons();
}

add_task(async function test_rich_priority() {
  const pageURI = uri("http://example.com/");
  await PlacesTestUtils.addVisits(pageURI);

  const favicon16File = do_get_file("favicon-normal16.png");
  const favicon16URI = uri("http://example.com/favicon16.png");
  const favicon16DataURI = await createDataURLForFavicon({
    data: readFileData(favicon16File),
    mimeType: "image/png",
  });
  await PlacesUtils.favicons.setFaviconForPage(
    pageURI,
    favicon16URI,
    favicon16DataURI,
    0,
    true // Rich
  );

  const favicon32File = do_get_file("favicon-normal32.png");
  const favicon32URI = uri("http://example.com/favicon32.png");
  const favicon32DataURI = await createDataURLForFavicon({
    data: readFileData(favicon32File),
    mimeType: "image/png",
  });
  await PlacesUtils.favicons.setFaviconForPage(
    pageURI,
    favicon32URI,
    favicon32DataURI,
    0,
    false // Non-rich
  );

  // Non-rich icon should be prioritieze for preferred width <= 64px.
  const result = await PlacesUtils.favicons.getFaviconForPage(pageURI, 8);
  Assert.equal(result.uri.spec, favicon32URI.spec);
  Assert.equal(result.dataURI.spec, favicon32DataURI.spec);
  Assert.equal(result.mimeType, "image/png");
  Assert.equal(result.width, 32);

  await PlacesUtils.history.clear();
  await PlacesTestUtils.clearFavicons();
});

add_task(async function test_rich_priority_above_threshold() {
  const pageURI = uri("http://example.com/");
  await PlacesTestUtils.addVisits(pageURI);

  const favicon16File = do_get_file("favicon-normal16.png");
  const favicon16URI = uri("http://example.com/favicon16.png");
  const favicon16DataURI = await createDataURLForFavicon({
    data: readFileData(favicon16File),
    mimeType: "image/png",
  });
  await PlacesUtils.favicons.setFaviconForPage(
    pageURI,
    favicon16URI,
    favicon16DataURI,
    0,
    false // Non-rich
  );

  const favicon32File = do_get_file("favicon-normal32.png");
  const favicon32URI = uri("http://example.com/favicon32.png");
  const favicon32DataURI = await createDataURLForFavicon({
    data: readFileData(favicon32File),
    mimeType: "image/png",
  });
  await PlacesUtils.favicons.setFaviconForPage(
    pageURI,
    favicon32URI,
    favicon32DataURI,
    0,
    true // Rich
  );

  // Non-rich icon should be prioritized for preferred width > 64px.
  const result = await PlacesUtils.favicons.getFaviconForPage(pageURI, 72);
  Assert.equal(result.uri.spec, favicon32URI.spec);
  Assert.equal(result.dataURI.spec, favicon32DataURI.spec);
  Assert.equal(result.mimeType, "image/png");
  Assert.equal(result.width, 32);

  await PlacesUtils.history.clear();
  await PlacesTestUtils.clearFavicons();
});

add_task(async function test_no_favicon() {
  const pageURI = uri("http://example.com/");
  const result = await PlacesUtils.favicons.getFaviconForPage(pageURI);
  Assert.equal(result, null);
});

add_task(async function test_invalid_page_uri() {
  const TEST_DATA = [
    {
      pageURI: null,
      expected: "NS_ERROR_ILLEGAL_VALUE",
    },
    {
      pageURI: "",
      expected: "NS_ERROR_XPC_BAD_CONVERT_JS",
    },
    {
      pageURI: "http://example.com",
      expected: "NS_ERROR_XPC_BAD_CONVERT_JS",
    },
  ];

  for (let { pageURI, expected } of TEST_DATA) {
    try {
      info(`Invalid page URI test for [${pageURI}]`);
      await PlacesUtils.favicons.getFaviconForPage(pageURI);
      Assert.ok(false, "Error should happened");
    } catch (e) {
      Assert.equal(
        e.name,
        expected,
        `Expected error happened for [${pageURI}]`
      );
    }
  }
});

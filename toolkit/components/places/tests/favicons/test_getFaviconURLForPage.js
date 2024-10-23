/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ICON32_URL = "http://places.test/favicon-normal32.png";

add_task(async function test_normal() {
  let pageURI = NetUtil.newURI("http://example.com/normal");

  await PlacesTestUtils.addVisits(pageURI);
  await PlacesTestUtils.setFaviconForPage(
    pageURI,
    SMALLPNG_DATA_URI,
    SMALLPNG_DATA_URI
  );

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      pageURI,
      function (aURI, aDataLen, aData, aMimeType) {
        Assert.ok(aURI.equals(SMALLPNG_DATA_URI));
        // Check also the expected data types.
        Assert.ok(aDataLen !== 0);
        Assert.ok(aData.length !== 0);
        Assert.ok(aMimeType === "image/png");
        resolve();
      }
    );
  });
});

add_task(async function test_missing() {
  PlacesTestUtils.clearFavicons();
  let pageURI = NetUtil.newURI("http://example.com/missing");

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconURLForPage(
      pageURI,
      function (aURI, aDataLen, aData, aMimeType) {
        // Check also the expected data types.
        Assert.ok(aURI === null);
        Assert.ok(aDataLen === 0);
        Assert.ok(aData.length === 0);
        Assert.ok(aMimeType === "");
        resolve();
      }
    );
  });
});

add_task(async function test_fallback() {
  const ROOT_URL = "https://www.example.com/";
  const ROOT_ICON_URL = ROOT_URL + "favicon.ico";
  const SUBPAGE_URL = ROOT_URL + "/missing";

  info("Set icon for the root");
  await PlacesTestUtils.addVisits(ROOT_URL);
  let dataURL = await readFileDataAsDataURL(
    do_get_file("favicon-normal16.png"),
    "image/png"
  );
  await PlacesTestUtils.setFaviconForPage(ROOT_URL, ROOT_ICON_URL, dataURL);

  info("check fallback icons");
  Assert.equal(
    await getFaviconUrlForPage(ROOT_URL),
    ROOT_ICON_URL,
    "The root should have its favicon"
  );
  Assert.equal(
    await getFaviconUrlForPage(SUBPAGE_URL),
    ROOT_ICON_URL,
    "The page should fallback to the root icon"
  );

  info("Now add a proper icon for the page");
  await PlacesTestUtils.addVisits(SUBPAGE_URL);
  let dataURL32 = await readFileDataAsDataURL(
    do_get_file("favicon-normal32.png"),
    "image/png"
  );
  await PlacesTestUtils.setFaviconForPage(SUBPAGE_URL, ICON32_URL, dataURL32);

  info("check no fallback icons");
  Assert.equal(
    await getFaviconUrlForPage(ROOT_URL),
    ROOT_ICON_URL,
    "The root should still have its favicon"
  );
  Assert.equal(
    await getFaviconUrlForPage(SUBPAGE_URL),
    ICON32_URL,
    "The page should also have its icon"
  );
});

add_task(async function test_URIsWithPort() {
  const URL_WITH_PORT = "https://www.example.com:5000/";
  const ICON_URL = URL_WITH_PORT + "favicon.ico";
  const URL_WITHOUT_PORT = "https://www.example.com/";
  const ICON_URL_NO_PORT = URL_WITHOUT_PORT + "favicon.ico";

  info("Set icon for the URL with the port");
  await PlacesTestUtils.addVisits(URL_WITH_PORT);
  let dataURL = await readFileDataAsDataURL(
    do_get_file("favicon-normal16.png"),
    "image/png"
  );
  await PlacesTestUtils.setFaviconForPage(URL_WITH_PORT, ICON_URL, dataURL);

  Assert.equal(
    await getFaviconUrlForPage(URL_WITH_PORT),
    ICON_URL,
    "The favicon of the URL with the port should be chosen"
  );

  info("Set icon for the URL without the port");
  await PlacesTestUtils.addVisits(URL_WITHOUT_PORT);
  let dataURL32 = await readFileDataAsDataURL(
    do_get_file("favicon-normal32.png"),
    "image/png"
  );
  await PlacesTestUtils.setFaviconForPage(
    URL_WITHOUT_PORT,
    ICON_URL_NO_PORT,
    dataURL32
  );

  Assert.equal(
    await getFaviconUrlForPage(URL_WITH_PORT),
    ICON_URL,
    "The favicon of the URL with the port should still be chosen when both are defined"
  );

  Assert.equal(
    await getFaviconUrlForPage(URL_WITHOUT_PORT),
    ICON_URL_NO_PORT,
    "The favicon of the URL without the port should be chosen correctly when there is an icon defined for the url with a port"
  );
});

add_task(async function test_noRootIconFallback() {
  await PlacesTestUtils.clearFavicons();
  const ROOT_URL = "http://test.com";
  const SUBPAGE_URLS = [
    ROOT_URL + "/page",
    ROOT_URL + "/about",
    ROOT_URL + "/home",
  ];

  for (let i = 0; i < 10; i++) {
    await PlacesTestUtils.addVisits(SUBPAGE_URLS[0]);
  }
  await PlacesTestUtils.addVisits(SUBPAGE_URLS[1]);
  await PlacesTestUtils.addVisits(SUBPAGE_URLS[2]);

  let dataURL32 = await readFileDataAsDataURL(
    do_get_file("favicon-normal32.png"),
    "image/png"
  );

  for (let url of SUBPAGE_URLS) {
    await PlacesTestUtils.setFaviconForPage(
      url,
      url + "/favicon.ico",
      dataURL32
    );
  }

  await PlacesTestUtils.addVisits(ROOT_URL);

  Assert.equal(
    await getFaviconUrlForPage(ROOT_URL),
    SUBPAGE_URLS[0] + "/favicon.ico",
    "No root icon, should use icon from most frecent subpage"
  );
});

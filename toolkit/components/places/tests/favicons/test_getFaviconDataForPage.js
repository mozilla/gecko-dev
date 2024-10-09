/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const FAVICON_URI = NetUtil.newURI(do_get_file("favicon-normal32.png"));
const FAVICON_DATA = readFileData(do_get_file("favicon-normal32.png"));
const FAVICON_MIMETYPE = "image/png";
const ICON32_URL = "http://places.test/favicon-normal32.png";

const FAVICON16_DATA = readFileData(do_get_file("favicon-normal16.png"));
const ICON16_URL = "http://places.test/favicon-normal16.png";

const FAVICON64_DATA = readFileData(do_get_file("favicon-big64.png"));
const ICON64_URL = "http://places.test/favicon-big64.png";

const SVG_DATA = readFileData(do_get_file("favicon-svg.svg"));
const SVG_URL = "http://example.com/favicon.svg";

const MIMETYPE_PNG = "image/png";
const MIMETYPE_SVG = "image/svg+xml";

add_task(async function test_normal() {
  Assert.equal(FAVICON_DATA.length, 344);
  let pageURI = NetUtil.newURI("http://example.com/normal");
  let dataURL = await PlacesTestUtils.fileDataToDataURL(
    FAVICON_DATA,
    "image/png"
  );

  await PlacesTestUtils.addVisits(pageURI);

  await PlacesUtils.favicons.setFaviconForPage(pageURI, FAVICON_URI, dataURL);

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      pageURI,
      function (aURI, aDataLen, aData, aMimeType) {
        Assert.ok(aURI.equals(FAVICON_URI));
        Assert.equal(FAVICON_DATA.length, aDataLen);
        Assert.ok(compareArrays(FAVICON_DATA, aData));
        Assert.equal(FAVICON_MIMETYPE, aMimeType);
        resolve();
      }
    );
  });
});

add_task(async function test_missing() {
  let pageURI = NetUtil.newURI("http://example.com/missing");

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
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
  let data = readFileData(do_get_file("favicon-normal16.png"));
  let dataURL = await PlacesTestUtils.fileDataToDataURL(data, "image/png");
  await PlacesTestUtils.setFaviconForPage(ROOT_URL, ROOT_ICON_URL, dataURL);

  info("check fallback icons");
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      NetUtil.newURI(ROOT_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ROOT_ICON_URL);
        Assert.equal(aDataLen, data.length);
        Assert.deepEqual(aData, data);
        Assert.equal(aMimeType, "image/png");
        resolve();
      }
    );
  });
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      NetUtil.newURI(SUBPAGE_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ROOT_ICON_URL);
        Assert.equal(aDataLen, data.length);
        Assert.deepEqual(aData, data);
        Assert.equal(aMimeType, "image/png");
        resolve();
      }
    );
  });

  info("Now add a proper icon for the page");
  await PlacesTestUtils.addVisits(SUBPAGE_URL);
  let data32 = readFileData(do_get_file("favicon-normal32.png"));
  let dataURL32 = await PlacesTestUtils.fileDataToDataURL(data32, "image/png");
  await PlacesTestUtils.setFaviconForPage(SUBPAGE_URL, ICON32_URL, dataURL32);

  info("check no fallback icons");
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      NetUtil.newURI(ROOT_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ROOT_ICON_URL);
        Assert.equal(aDataLen, data.length);
        Assert.deepEqual(aData, data);
        Assert.equal(aMimeType, "image/png");
        resolve();
      }
    );
  });
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      NetUtil.newURI(SUBPAGE_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ICON32_URL);
        Assert.equal(aDataLen, data32.length);
        Assert.deepEqual(aData, data32);
        Assert.equal(aMimeType, "image/png");
        resolve();
      }
    );
  });
});

add_task(async function test_richIconPrioritizationBelowThreshold() {
  await runFaviconTest(
    "https://example.com/test_prioritization_below_threshold",
    [
      {
        url: ICON16_URL,
        data: FAVICON16_DATA,
        isRich: false,
        mimetype: MIMETYPE_PNG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: true,
        mimetype: MIMETYPE_PNG,
      },
    ],
    ICON16_URL,
    FAVICON16_DATA,
    12
  );
});

add_task(async function test_richIconPrioritizationAboveThreshold() {
  await runFaviconTest(
    "https://example.com/test_prioritization_below_threshold",
    [
      {
        url: ICON16_URL,
        data: FAVICON16_DATA,
        isRich: false,
        mimetype: MIMETYPE_PNG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: true,
        mimetype: MIMETYPE_PNG,
      },
    ],
    ICON32_URL,
    FAVICON_DATA,
    72
  );
});

add_task(async function test_sizeSelection() {
  // Icons set:
  //  - 16px non-rich
  //  - 64px non-rich
  //  - 32px rich
  const testCases = [
    // Should select 16px icon, non-rich icons are prioritized for
    // preferred size <= 64px, and (24 - 16) = 8 <= (64 - 24) / 4 = 10.
    // Therefore smaller icon is selected.
    {
      expectedURI: ICON16_URL,
      expectedData: FAVICON16_DATA,
      preferredSize: 24,
    },
    // Should select 64px icon, non-rich icons are prioritized for
    // preferred size <= 64px, and (64 - 32) / 4 = 8 < (32 - 16) = 16.
    // Therefore, larger icon is selected.
    {
      expectedURI: ICON64_URL,
      expectedData: FAVICON64_DATA,
      preferredSize: 32,
    },
    // Should select 64px icon, no discrimination between rich/non-rich for
    // preferred size > 64px.
    {
      expectedURI: ICON64_URL,
      expectedData: FAVICON64_DATA,
      preferredSize: 80,
    },
  ];

  for (const { expectedURI, expectedData, preferredSize } of testCases) {
    await runFaviconTest(
      "https://example.com/test_size_selection",
      [
        {
          url: ICON16_URL,
          data: FAVICON16_DATA,
          isRich: false,
          mimetype: MIMETYPE_PNG,
        },
        {
          url: ICON64_URL,
          data: FAVICON64_DATA,
          isRich: false,
          mimetype: MIMETYPE_PNG,
        },
        {
          url: ICON32_URL,
          data: FAVICON_DATA,
          isRich: true,
          mimetype: MIMETYPE_PNG,
        },
      ],
      expectedURI,
      expectedData,
      preferredSize
    );
  }
});

add_task(async function test_sizeSelectionRichOnly() {
  // Should select 16px icon, since there are no non-rich icons found,
  // we return the best-sized rich icon.
  await runFaviconTest(
    "https://example.com/test_size_selection_rich_only",
    [
      {
        url: ICON16_URL,
        data: FAVICON16_DATA,
        isRich: true,
        mimetype: MIMETYPE_PNG,
      },
      {
        url: ICON64_URL,
        data: FAVICON64_DATA,
        isRich: true,
        mimetype: MIMETYPE_PNG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: true,
        mimetype: MIMETYPE_PNG,
      },
    ],
    ICON16_URL,
    FAVICON16_DATA,
    17
  );
});

add_task(async function test_svg() {
  // Selected non-svg is not a perfect fit, so we prefer SVG.
  await runFaviconTest(
    "https://example.com/test_icon_selection_svg",
    [
      {
        url: SVG_URL,
        data: SVG_DATA,
        isRich: false,
        mimetype: MIMETYPE_SVG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: false,
        mimetype: MIMETYPE_PNG,
      },
    ],
    SVG_URL,
    SVG_DATA,
    31
  );

  // Selected non-svg is a perfect fit, so we prefer it.
  await runFaviconTest(
    "https://example.com/test_icon_selection_svg",
    [
      {
        url: SVG_URL,
        data: SVG_DATA,
        isRich: false,
        mimetype: MIMETYPE_SVG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: false,
        mimetype: MIMETYPE_PNG,
      },
    ],
    ICON32_URL,
    FAVICON_DATA,
    32
  );

  // Selected non-svg is a perfect fit, but it is rich so we prefer SVG.
  await runFaviconTest(
    "https://example.com/test_icon_selection_svg",
    [
      {
        url: SVG_URL,
        data: SVG_DATA,
        isRich: false,
        mimetype: MIMETYPE_SVG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: true,
        mimetype: MIMETYPE_PNG,
      },
    ],
    SVG_URL,
    SVG_DATA,
    32
  );

  // Selected non-svg is not a perfect fit, but SVG is rich, so we prefer non-SVG.
  await runFaviconTest(
    "https://example.com/test_icon_selection_svg",
    [
      {
        url: SVG_URL,
        data: SVG_DATA,
        isRich: true,
        mimetype: MIMETYPE_SVG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: false,
        mimetype: MIMETYPE_PNG,
      },
    ],
    ICON32_URL,
    FAVICON_DATA,
    31
  );

  // Selected non-SVG is a perfect fit and it is rich, and SVG is also rich,
  // so we prefer the original non-SVG selection.
  await runFaviconTest(
    "https://example.com/test_icon_selection_svg",
    [
      {
        url: SVG_URL,
        data: SVG_DATA,
        isRich: true,
        mimetype: MIMETYPE_SVG,
      },
      {
        url: ICON32_URL,
        data: FAVICON_DATA,
        isRich: true,
        mimetype: MIMETYPE_PNG,
      },
    ],
    ICON32_URL,
    FAVICON_DATA,
    32
  );

  // When requested size is above threshold we have no preference when it comes to richness.
  await runFaviconTest(
    "https://example.com/test_icon_selection_svg",
    [
      {
        url: SVG_URL,
        data: SVG_DATA,
        isRich: true,
        mimetype: MIMETYPE_SVG,
      },
      {
        url: ICON64_URL,
        data: FAVICON64_DATA,
        isRich: false,
        mimetype: MIMETYPE_PNG,
      },
    ],
    SVG_URL,
    SVG_DATA,
    65
  );

  // Prefer non-rich SVG when requested size is below threshold.
  await runFaviconTest(
    "https://example.com/test_icon_selection_svg",
    [
      {
        url: SVG_URL + "#2",
        data: SVG_DATA,
        isRich: true,
        mimetype: MIMETYPE_SVG,
      },
      {
        url: SVG_URL,
        data: SVG_DATA,
        isRich: false,
        mimetype: MIMETYPE_SVG,
      },
    ],
    SVG_URL,
    SVG_DATA,
    32
  );
});

async function runFaviconTest(
  PAGE_URL,
  iconData,
  expectedURI,
  expectedData,
  preferredSize
) {
  await PlacesTestUtils.clearFavicons();
  await PlacesTestUtils.addVisits(PAGE_URL);

  for (const { url, data, isRich, mimetype } of iconData) {
    const dataURL = await PlacesTestUtils.fileDataToDataURL(data, mimetype);
    await PlacesTestUtils.setFaviconForPage(PAGE_URL, url, dataURL, 0, isRich);
  }

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(
      NetUtil.newURI(PAGE_URL),
      (aURI, aDataLen, aData) => {
        Assert.equal(aURI.spec, expectedURI);
        Assert.equal(aDataLen, expectedData.length);
        Assert.deepEqual(aData, expectedData);
        resolve();
      },
      preferredSize
    );
  });
}

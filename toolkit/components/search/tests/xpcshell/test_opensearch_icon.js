/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ICON_TESTS = [
  {
    name: "Big Icon",
    image: "bigIcon.ico",
    expected: "data:image/png;base64,",
  },
  {
    name: "Remote Icon",
    image: "remoteIcon.ico",
    expected: "data:image/x-icon;base64,",
  },
  {
    name: "SVG Icon",
    image: "svgIcon.svg",
    expected: "data:image/svg+xml;base64,",
  },
];

let ENGINE_NO_ICONS;

add_setup(async function () {
  let server = useHttpServer("");
  server.registerContentType("sjs", "sjs");
  ENGINE_NO_ICONS = `${gHttpURL}/opensearch/fr-domain-iso8859-1.xml`;
  await Services.search.init();
});

add_task(async function test_icon_types() {
  for (let test of ICON_TESTS) {
    info(`Testing ${test.name}`);

    let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.ADDED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
      SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
      SearchUtils.TOPIC_ENGINE_MODIFIED
    );
    const engineData = {
      baseURL: `${gHttpURL}/data/`,
      imageURL: `${gHttpURL}/icons/${test.image}`,
      name: test.name,
      method: "GET",
    };
    // The easiest way to test adding the icon is via a generated xml, otherwise
    // we have to somehow insert the address of the server into it.
    SearchTestUtils.installOpenSearchEngine({
      url: `${gHttpURL}/sjs/engineMaker.sjs?${JSON.stringify(engineData)}`,
    });
    let engine = await promiseEngineAdded;
    // Ensure this is a nsISearchEngine.
    engine.QueryInterface(Ci.nsISearchEngine);
    await promiseIconChanged;

    Assert.ok(await engine.getIconURL(), `${test.name} engine has an icon`);
    Assert.ok(
      (await engine.getIconURL()).startsWith(test.expected),
      `${test.name} iconURI starts with the expected information`
    );
  }
});

add_task(async function test_multiple_icons_in_file() {
  let engine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/opensearch/images.xml`,
  });

  Assert.ok(
    (await engine.getIconURL()).includes("ico16"),
    "Default should be 16."
  );
  info("Available dimensions should return the exact icon.");
  Assert.ok((await engine.getIconURL(16)).includes("ico16"));
  Assert.ok((await engine.getIconURL(32)).includes("ico32"));
  Assert.ok((await engine.getIconURL(256)).includes("ico256"));

  info("Other dimensions should return the closest icon.");
  Assert.ok((await engine.getIconURL(257)).includes("ico256"));
  Assert.ok((await engine.getIconURL(255)).includes("ico256"));
  Assert.ok((await engine.getIconURL(33)).includes("ico32"));
  Assert.ok((await engine.getIconURL(31)).includes("ico32"));
  Assert.ok((await engine.getIconURL(17)).includes("ico16"));
  Assert.ok((await engine.getIconURL(15)).includes("ico16"));

  Assert.ok((await engine.getIconURL(77)).includes("ico256"));
  Assert.ok((await engine.getIconURL(76)).includes("ico32"));
});

add_task(async function test_icon_not_in_opensearch_file_invalid_svg() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let engine = await Services.search.addOpenSearchEngine(
    ENGINE_NO_ICONS,
    // We still add the icon even if we cannot determine the size.
    "data:image/svg+xml;base64,invalid+svg"
  );

  await promiseIconChanged;
  let sizes = Object.keys(engine.wrappedJSObject._iconMapObj);
  Assert.deepEqual(sizes, ["16"], "Defaulted to 16x16");

  await Services.search.removeEngine(engine);
});

add_task(async function test_icon_not_in_opensearch_file_invalid_ico() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let engine = await Services.search.addOpenSearchEngine(
    ENGINE_NO_ICONS,
    // We still add the icon even if we cannot determine the size.
    "data:image/x-icon;base64,invalid+ico"
  );

  await promiseIconChanged;
  let sizes = Object.keys(engine.wrappedJSObject._iconMapObj);
  Assert.deepEqual(sizes, ["16"], "Defaulted to 16x16");

  await Services.search.removeEngine(engine);
});

add_task(async function test_icon_not_in_opensearch_file_svg() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let icoIconDataUrl = await SearchTestUtils.fetchAsDataUrl(
    `${gHttpURL}/icons/svgIcon.svg`
  );

  let engine = await Services.search.addOpenSearchEngine(
    ENGINE_NO_ICONS,
    icoIconDataUrl
  );

  await promiseIconChanged;
  let sizes = Object.keys(engine.wrappedJSObject._iconMapObj);
  Assert.deepEqual(sizes, ["16"], "Icon size was correctly detected.");
  Assert.equal(await engine.getIconURL(16), icoIconDataUrl, "Correct icon");
  await Services.search.removeEngine(engine);
});

add_task(async function test_icon_not_in_opensearch_file_ico() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let icoIconDataUrl = await SearchTestUtils.fetchAsDataUrl(
    `${gHttpURL}/icons/multipleSizes.ico`
  );

  let engine = await Services.search.addOpenSearchEngine(
    ENGINE_NO_ICONS,
    icoIconDataUrl
  );

  await promiseIconChanged;
  let sizes = Object.keys(engine.wrappedJSObject._iconMapObj);
  Assert.deepEqual(sizes, ["32"], "Icon size was correctly detected.");
  Assert.equal(await engine.getIconURL(32), icoIconDataUrl, "Correct icon");
  await Services.search.removeEngine(engine);
});

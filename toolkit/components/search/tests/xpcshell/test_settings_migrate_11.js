/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests search settings migration from version 10 to latest.
// In version 11 the keys of _iconMapObj are converted to the width.
// In version 12 the _iconURL of engines is moved into _iconMapObj.

let SEARCH_SETTINGS = {
  version: 10,
  metaData: {
    useSavedOrder: true,
    defaultEngineId: "engine1",
  },
  engines: [
    {
      id: "engine1",
      _name: "engine1",
      _isAppProvided: true,
      _metaData: { order: 1 },
    },
    {
      id: "29f30c5a-627d-46ba-b6fd-fdbe56731484",
      _name: "IconsTest",
      _loadPath: "[http]127.0.0.1/iconstest.xml",
      description: "IconsTest. Search by Test.",
      _iconURL: "placeholder",
      _iconMapObj: {
        '{"width":32,"height":32}': "data:image/x-icon;base64,ico32",
        '{"width":74,"height":74}': "data:image/png;base64,ico74",
        '{"width":42,"height":41}': "data:image/png;base64,ico42",
        "{}": "data:image/png;base64,ico0",
        "invalid json": "data:image/png;base64,ico0",
        // Going back and forth should work.
        64: "data:image/png;base64,ico64",
      },
      _metaData: {
        loadPathHash: "OixanEC3I3fSEnZY/YeX1YndC9qdzkqotEEKsodghLY=",
        order: 2,
      },
      _urls: [
        {
          params: [
            { name: "q", value: "{searchTerms}" },
            { name: "form", value: "MOZW" },
          ],
          rels: [],
          template: "http://www.bing.com/search",
        },
      ],
      _orderHint: null,
      _telemetryId: null,
      _definedAliases: [],
      _updateInterval: null,
      _updateURL: null,
      _iconUpdateURL: null,
    },
  ],
};

const CONFIG = [{ identifier: "engine1" }];
let icon16;

add_setup(async function () {
  useHttpServer();
  icon16 = await SearchTestUtils.fetchAsDataUrl(
    `${gHttpURL}/icons/remoteIcon.ico`
  );
  SEARCH_SETTINGS.engines[1]._iconURL = icon16;
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);

  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME),
    SEARCH_SETTINGS,
    { compress: true }
  );

  await Services.search.init();
});

add_task(async function test_icon_migration() {
  let engine = Services.search.getEngineByName("IconsTest");
  let sizes = Object.keys(engine.wrappedJSObject._iconMapObj);

  Assert.equal(
    sizes.length,
    4,
    "Only valid _iconMapObj keys and _iconURL got converted."
  );
  Assert.ok(
    sizes.map(k => parseInt(k)).every(k => !isNaN(k)),
    "_iconMapObj keys got converted to numbers."
  );

  info("Checking if the icons are correct.");
  Assert.equal(await engine.getIconURL(), icon16);
  Assert.equal(await engine.getIconURL(16), icon16);
  Assert.ok((await engine.getIconURL(32)).includes("ico32"));
  Assert.ok((await engine.getIconURL(64)).includes("ico64"));
  Assert.ok((await engine.getIconURL(74)).includes("ico74"));
});

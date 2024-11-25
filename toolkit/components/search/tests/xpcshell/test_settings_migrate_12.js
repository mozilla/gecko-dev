/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests search settings migration from version 11 to latest.
// In version 12 the _iconURL of engines is moved into _iconMapObj.

const SEARCH_SETTINGS = {
  version: 11,
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
        16: "data:image/x-icon;base64,ico16",
        32: "data:image/x-icon;base64,ico32",
        74: "data:image/png;base64,ico74",
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

add_task(async function test_migration() {
  let engine = Services.search.getEngineByName("IconsTest");
  let iconMapObj = engine.wrappedJSObject._iconMapObj;
  Assert.equal(
    Object.keys(iconMapObj).length,
    3,
    "Still 3 icons in _iconMapObj"
  );

  Assert.equal(
    await engine.getIconURL(16),
    icon16,
    "_iconURL overwrote the _iconMapObj icon"
  );
  info("Checking if the other icons are correct.");
  Assert.ok((await engine.getIconURL(32)).includes("ico32"));
  Assert.ok((await engine.getIconURL(74)).includes("ico74"));
});

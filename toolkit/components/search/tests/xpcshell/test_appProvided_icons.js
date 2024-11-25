/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that icons for application provided engines are correctly
 * loaded from remote settings.
 */

"use strict";

// A skeleton configuration that gets filled in from TESTS during `add_setup`.
let TESTS = [
  {
    engineId: "engine_no_icon",
    expectedIcon: null,
    expectedMimeType: null,
  },
  {
    engineId: "engine_exact_match",
    icons: [
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: ["engine_exact_match"],
        imageSize: 16,
        mimeType: "image/x-icon",
      },
    ],
  },
  {
    engineId: "engine_begins_with",
    icons: [
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: ["engine_begins*"],
        imageSize: 16,
        mimeType: "image/x-icon",
      },
    ],
  },
  {
    engineId: "engine_non_default_sized_icon",
    icons: [
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: [
          // This also tests whether multiple engine idenifiers work.
          "enterprise_shuttle",
          "engine_non_default_sized_icon",
        ],
        imageSize: 32,
        mimeType: "image/x-icon",
      },
    ],
  },
  {
    engineId: "engine_multiple_icons",
    icons: [
      {
        filename: "bigIcon.ico",
        engineIdentifiers: ["engine_multiple_icons"],
        imageSize: 16,
        mimeType: "image/x-icon",
      },
      {
        filename: "remoteIcon.ico",
        engineIdentifiers: ["engine_multiple_icons"],
        imageSize: 32,
        mimeType: "image/x-icon",
      },
      {
        filename: "svgIcon.svg",
        engineIdentifiers: ["engine_multiple_icons"],
        imageSize: 64,
        mimeType: "image/svg+xml",
      },
    ],
  },
  {
    engineId: "engine_svg_icon",
    icons: [
      {
        filename: "svgIcon.svg",
        engineIdentifiers: ["engine_svg_icon"],
        imageSize: 16,
        mimeType: "image/svg+xml",
      },
    ],
  },
];

add_setup(async function () {
  let client = RemoteSettings("search-config-icons");
  let db = client.db;

  await db.clear();

  let partialConfig = [];

  for (let test of TESTS) {
    partialConfig.push({ identifier: test.engineId });

    if ("icons" in test) {
      for (let icon of test.icons) {
        await insertRecordIntoCollection(client, { ...icon });
      }
    }
  }

  SearchTestUtils.setRemoteSettingsConfig(partialConfig);
  await Services.search.init();
});

for (let test of TESTS) {
  add_task(async function () {
    info("Testing engine: " + test.engineId);

    let engine = Services.search.getEngineById(test.engineId);
    if (test.icons) {
      for (let icon of test.icons) {
        let engineIconURL = await engine.getIconURL(icon.imageSize);
        Assert.notEqual(
          engineIconURL,
          null,
          "Should have an icon URL for the engine."
        );

        let response = await fetch(engineIconURL);
        let buffer = new Uint8Array(await response.arrayBuffer());

        let expectedBuffer = new Uint8Array(
          await getFileDataBuffer(icon.filename)
        );

        Assert.equal(
          buffer.length,
          expectedBuffer.length,
          "Should have received matching buffer lengths for the expected icon"
        );
        Assert.ok(
          buffer.every((value, index) => value === expectedBuffer[index]),
          "Should have received matching data for the expected icon"
        );

        let contentType = response.headers.get("content-type");

        Assert.equal(
          contentType,
          icon.mimeType,
          "Should have received matching MIME types for the expected icon"
        );

        Assert.equal(
          engineIconURL,
          await engine.getIconURL(icon.imageSize + 1),
          "Should choose closest icon."
        );
        Assert.equal(
          engineIconURL,
          await engine.getIconURL(icon.imageSize - 1),
          "Should choose closest icon."
        );

        if (icon.imageSize == 16) {
          Assert.equal(
            engineIconURL,
            await engine.getIconURL(),
            "Should default to 16x16."
          );
        }
      }
    } else {
      Assert.equal(
        await engine.getIconURL(),
        null,
        "Should not have an icon URL for the engine."
      );
    }
  });
}

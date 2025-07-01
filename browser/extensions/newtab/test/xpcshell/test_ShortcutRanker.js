"use strict";

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

add_task(async function test_tsampleTopSites_no_guid_last() {
  // Ranker are utilities we are testing
  const Ranker = ChromeUtils.importESModule(
    "resource://newtab/lib/ShortcutsRanker.sys.mjs"
  );
  // We are going to stub a database call
  const { NewTabUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/NewTabUtils.sys.mjs"
  );

  await NewTabUtils.init();

  const sandbox = sinon.createSandbox();

  // Stub DB call
  sandbox
    .stub(NewTabUtils.activityStreamProvider, "executePlacesQuery")
    .resolves([
      ["a", 5, 10],
      ["b", 2, 10],
    ]);

  // First item here intentially has no guid
  const input = [
    { url: "no-guid.com" },
    { guid: "a", url: "a.com" },
    { guid: "b", url: "b.com" },
  ];

  const result = await Ranker.tsampleTopSites(input);

  Assert.ok(Array.isArray(result), "returns an array");
  Assert.equal(
    result[result.length - 1].url,
    "no-guid.com",
    "top-site without GUID is last"
  );

  sandbox.restore();
});

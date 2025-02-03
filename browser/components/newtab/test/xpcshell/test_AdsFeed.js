/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AdsFeed } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/AdsFeed.sys.mjs"
);

const { actionCreators: ac, actionTypes: at } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Actions.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const PREF_UNIFIED_ADS_ADSFEED_ENABLED = "unifiedAds.adsFeed.enabled";
const PREF_UNIFIED_ADS_ADSFEED_TILES_ENABLED =
  "unifiedAds.adsFeed.tiles.enabled";
const PREF_UNIFIED_ADS_TILES_ENABLED = "unifiedAds.tiles.enabled";
const PREF_UNIFIED_ADS_ENDPOINT = "unifiedAds.endpoint";
const PREF_UNIFIED_ADS_PLACEMENTS = "discoverystream.placements.tiles";
const PREF_UNIFIED_ADS_COUNTS = "discoverystream.placements.tiles.counts";
const PREF_UNIFIED_ADS_BLOCKED_LIST = "unifiedAds.blockedAds";

// Primary pref that is toggled when enabling top site sponsored tiles
const PREF_FEED_TOPSITES = "feeds.topsites";
const PREF_SHOW_SPONSORED_TOPSITES = "showSponsoredTopSites";

// Primary pref that is toggled when enabling sponsored stories
const PREF_FEED_SECTIONS_TOPSTORIES = "feeds.section.topstories";
const PREF_SHOW_SPONSORED = "showSponsored";
const PREF_SYSTEM_SHOW_SPONSORED = "system.showSponsored";

const mockedTileData = [
  {
    url: "https://www.test.com",
    image_url: "images/test-com.png",
    click_url: "https://www.test-click.com",
    impression_url: "https://www.test-impression.com",
    name: "test",
  },
  {
    url: "https://www.test1.com",
    image_url: "images/test1-com.png",
    click_url: "https://www.test1-click.com",
    impression_url: "https://www.test1-impression.com",
    name: "test1",
  },
];

function getAdsFeedForTest() {
  let feed = new AdsFeed();
  let tiles = mockedTileData;

  feed.store = {
    dispatch: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      tiles,
      lastUpdated: 1,
      Prefs: {
        values: {
          [PREF_UNIFIED_ADS_PLACEMENTS]:
            "newtab_tile_1, newtab_tile_2, newtab_tile_3",
          [PREF_UNIFIED_ADS_COUNTS]: "1, 1, 1",
          [PREF_UNIFIED_ADS_BLOCKED_LIST]: "",
          [PREF_UNIFIED_ADS_ENDPOINT]: "https://example.com",
          // AdsFeed/UAPI specific prefs to test
          [PREF_UNIFIED_ADS_TILES_ENABLED]: false,
          [PREF_UNIFIED_ADS_ADSFEED_ENABLED]: false,
          [PREF_UNIFIED_ADS_ADSFEED_TILES_ENABLED]: false,
          // Default display prefs for tiles/spocs
          [PREF_FEED_TOPSITES]: true,
          [PREF_SHOW_SPONSORED_TOPSITES]: true,
          [PREF_FEED_SECTIONS_TOPSTORIES]: true,
          [PREF_SHOW_SPONSORED]: true,
          [PREF_SYSTEM_SHOW_SPONSORED]: true,
        },
      },
    },
  };

  return feed;
}

add_task(async function test_construction() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(AdsFeed.prototype, "PersistentCache").returns({
    set: () => {},
    get: () => {},
  });

  let feed = new AdsFeed();

  info("AdsFeed constructor should create initial values");

  Assert.ok(feed, "Could construct a AdsFeed");
  Assert.ok(feed.loaded === false, "AdsFeed is not loaded");
  Assert.ok(feed.enabled === false, "AdsFeed is not enabled");
  Assert.ok(feed.lastUpdated === null, "AdsFeed has no lastUpdated record");
  Assert.ok(
    feed.tiles.length === 0,
    "tiles is initialized as a array with length of 0"
  );
  sandbox.restore();
});

add_task(async function test_isEnabled_tiles() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(AdsFeed.prototype, "PersistentCache").returns({
    set: () => {},
    get: () => {},
  });
  const dateNowTestValue = 1;
  sandbox.stub(AdsFeed.prototype, "Date").returns({
    now: () => dateNowTestValue,
  });

  let feed = getAdsFeedForTest(sandbox);

  feed.store.state.Prefs.values[PREF_UNIFIED_ADS_ADSFEED_ENABLED] = true;
  feed.store.state.Prefs.values[PREF_UNIFIED_ADS_ADSFEED_TILES_ENABLED] = true;
  feed.store.state.Prefs.values[PREF_UNIFIED_ADS_TILES_ENABLED] = true;

  feed.store.state.Prefs.values[PREF_FEED_TOPSITES] = true;
  feed.store.state.Prefs.values[PREF_SHOW_SPONSORED_TOPSITES] = true;
  feed.store.state.Prefs.values[PREF_FEED_SECTIONS_TOPSTORIES] = true;
  feed.store.state.Prefs.values[PREF_SHOW_SPONSORED] = true;
  feed.store.state.Prefs.values[PREF_SYSTEM_SHOW_SPONSORED] = true;

  Assert.ok(feed.isEnabled());

  sandbox.restore();
});

add_task(async function test_onAction_INIT_tiles() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(AdsFeed.prototype, "PersistentCache").returns({
    set: () => {},
    get: () => {},
  });
  const dateNowTestValue = 1;
  sandbox.stub(AdsFeed.prototype, "Date").returns({
    now: () => dateNowTestValue,
  });

  let feed = getAdsFeedForTest(sandbox);

  feed.store.state.Prefs.values[PREF_UNIFIED_ADS_ADSFEED_ENABLED] = true;
  feed.store.state.Prefs.values[PREF_UNIFIED_ADS_ADSFEED_TILES_ENABLED] = true;
  feed.store.state.Prefs.values[PREF_UNIFIED_ADS_TILES_ENABLED] = true;

  sandbox.stub(feed, "isEnabled").returns(true);

  sandbox
    .stub(feed, "fetchData")
    .returns({ tiles: mockedTileData, lastUpdated: dateNowTestValue });

  info("AdsFeed.onAction INIT should initialize Ads");

  await feed.onAction({
    type: at.INIT,
  });

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWith(
      ac.BroadcastToContent({
        type: "ADS_UPDATE_DATA",
        data: {
          tiles: mockedTileData,
        },
        meta: {
          isStartup: true,
        },
      })
    )
  );

  sandbox.restore();
});

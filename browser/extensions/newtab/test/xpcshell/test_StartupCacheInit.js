/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  actionCreators: "resource://newtab/common/Actions.mjs",
  actionTypes: "resource://newtab/common/Actions.mjs",
  StartupCacheInit: "resource://newtab/lib/StartupCacheInit.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

add_task(async function test_construction() {
  let sandbox = sinon.createSandbox();

  let feed = new StartupCacheInit();

  info("StartupCacheInit constructor should create initial values");

  Assert.ok(feed, "Could construct a StartupCacheInit");
  Assert.ok(!feed.loaded, "StartupCacheInit is not loaded");
  Assert.ok(!feed.TopsitesUpdatedReply, "topsites have not replied");
  Assert.ok(!feed.DiscoveryStreamSpocsUpdateReply, "spocs have not replied");
  Assert.ok(!feed.WeatherUpdateReply, "weather has not replied");
  Assert.ok(
    !feed.CustomWallpaperUpdateReply,
    "custom wallpapers have not replied"
  );
  sandbox.restore();
});

add_task(async function test_onAction_INIT() {
  let sandbox = sinon.createSandbox();

  let feed = new StartupCacheInit();

  info("StartupCacheInit.onAction INIT should set loaded");

  await feed.onAction({
    type: actionTypes.INIT,
  });

  Assert.ok(feed.loaded);
  sandbox.restore();
});

add_task(
  async function test_onAction_NEW_TAB_STATE_REQUEST_WITHOUT_STARTUPCACHE() {
    let sandbox = sinon.createSandbox();

    let feed = new StartupCacheInit();

    feed.store = {
      dispatch: sinon.spy(),
      uninitFeed: sinon.spy(),
    };

    info(
      "StartupCacheInit.onAction NEW_TAB_STATE_REQUEST_WITHOUT_STARTUPCACHE should uninit"
    );

    await feed.onAction({
      type: actionTypes.INIT,
    });
    await feed.onAction({
      type: actionTypes.NEW_TAB_STATE_REQUEST_WITHOUT_STARTUPCACHE,
    });

    Assert.ok(feed.store.uninitFeed.calledOnce);
    Assert.ok(
      feed.store.uninitFeed.calledWith("feeds.startupcacheinit", {
        type: actionTypes.UNINIT,
      })
    );
    sandbox.restore();
  }
);

add_task(async function test_onAction_NEW_TAB_STATE_REQUEST_STARTUPCACHE() {
  let sandbox = sinon.createSandbox();

  let feed = new StartupCacheInit();

  feed.store = {
    dispatch: sinon.spy(),
    uninitFeed: sinon.spy(),
  };

  info(
    "StartupCacheInit.onAction NEW_TAB_STATE_REQUEST_STARTUPCACHE should uninit"
  );

  await feed.onAction({
    type: actionTypes.INIT,
  });
  await feed.onAction({
    type: actionTypes.NEW_TAB_STATE_REQUEST_STARTUPCACHE,
    meta: { fromTarget: "fromTarget" },
  });

  Assert.ok(feed.store.uninitFeed.calledOnce);
  Assert.ok(
    feed.store.uninitFeed.calledWith("feeds.startupcacheinit", {
      type: actionTypes.UNINIT,
    })
  );
  sandbox.restore();
});

add_task(async function test_onAction_TOP_SITES_UPDATED() {
  let sandbox = sinon.createSandbox();

  let feed = new StartupCacheInit();

  feed.store = {
    dispatch: sinon.spy(),
    uninitFeed: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      TopSites: {
        rows: ["topsite1", "topsite2"],
      },
    },
  };

  info(
    "StartupCacheInit.onAction TOP_SITES_UPDATED should forward topsites info"
  );

  await feed.onAction({
    type: actionTypes.INIT,
  });
  await feed.onAction({
    type: actionTypes.TOP_SITES_UPDATED,
  });
  await feed.onAction({
    type: actionTypes.NEW_TAB_STATE_REQUEST_STARTUPCACHE,
    meta: { fromTarget: "fromTarget" },
  });

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWith(
      actionCreators.OnlyToOneContent(
        {
          type: actionTypes.TOP_SITES_UPDATED,
          data: {
            links: ["topsite1", "topsite2"],
          },
        },
        "fromTarget"
      )
    )
  );
  sandbox.restore();
});

add_task(async function test_onAction_DISCOVERY_STREAM_SPOCS_UPDATE() {
  let sandbox = sinon.createSandbox();

  let feed = new StartupCacheInit();

  feed.store = {
    dispatch: sinon.spy(),
    uninitFeed: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      DiscoveryStream: {
        spocs: {
          data: ["spoc1", "spoc2"],
          lastUpdated: "lastUpdated",
        },
      },
      Prefs: {
        values: {
          "discoverystream.spocs.startupCache.enabled": false,
        },
      },
    },
  };

  info(
    "StartupCacheInit.onAction DISCOVERY_STREAM_SPOCS_UPDATE should forward spocs info"
  );

  await feed.onAction({
    type: actionTypes.INIT,
  });
  await feed.onAction({
    type: actionTypes.DISCOVERY_STREAM_SPOCS_UPDATE,
  });
  await feed.onAction({
    type: actionTypes.NEW_TAB_STATE_REQUEST_STARTUPCACHE,
    meta: { fromTarget: "fromTarget" },
  });

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWith(
      actionCreators.OnlyToOneContent(
        {
          type: actionTypes.DISCOVERY_STREAM_SPOCS_UPDATE,
          data: {
            spocs: ["spoc1", "spoc2"],
            lastUpdated: "lastUpdated",
          },
        },
        "fromTarget"
      )
    )
  );
  sandbox.restore();
});

add_task(async function test_onAction_WEATHER_UPDATE() {
  let sandbox = sinon.createSandbox();

  let feed = new StartupCacheInit();

  feed.store = {
    dispatch: sinon.spy(),
    uninitFeed: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      Weather: {
        suggestions: "suggestions",
        lastUpdated: "lastUpdated",
        locationData: "locationData",
      },
    },
  };

  info("StartupCacheInit.onAction WEATHER_UPDATE should forward weather info");

  await feed.onAction({
    type: actionTypes.INIT,
  });
  await feed.onAction({
    type: actionTypes.WEATHER_UPDATE,
  });
  await feed.onAction({
    type: actionTypes.NEW_TAB_STATE_REQUEST_STARTUPCACHE,
    meta: { fromTarget: "fromTarget" },
  });

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWith(
      actionCreators.OnlyToOneContent(
        {
          type: actionTypes.WEATHER_UPDATE,
          data: {
            suggestions: "suggestions",
            lastUpdated: "lastUpdated",
            locationData: "locationData",
          },
        },
        "fromTarget"
      )
    )
  );
  sandbox.restore();
});

add_task(async function test_onAction_WALLPAPERS_CUSTOM_SET() {
  let sandbox = sinon.createSandbox();

  let feed = new StartupCacheInit();

  feed.store = {
    dispatch: sinon.spy(),
    uninitFeed: sinon.spy(),
    getState() {
      return this.state;
    },
    state: {
      Wallpapers: {
        uploadedWallpaper: "uploadedWallpaper",
      },
    },
  };

  info(
    "StartupCacheInit.onAction WALLPAPERS_CUSTOM_SET should forward wallpaper info"
  );

  await feed.onAction({
    type: actionTypes.INIT,
  });
  await feed.onAction({
    type: actionTypes.WALLPAPERS_CUSTOM_SET,
  });
  await feed.onAction({
    type: actionTypes.NEW_TAB_STATE_REQUEST_STARTUPCACHE,
    meta: { fromTarget: "fromTarget" },
  });

  Assert.ok(feed.store.dispatch.calledOnce);
  Assert.ok(
    feed.store.dispatch.calledWith(
      actionCreators.OnlyToOneContent(
        {
          type: actionTypes.WALLPAPERS_CUSTOM_SET,
          data: "uploadedWallpaper",
        },
        "fromTarget"
      )
    )
  );
  sandbox.restore();
});

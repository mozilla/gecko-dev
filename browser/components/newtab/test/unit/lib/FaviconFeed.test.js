"use strict";
import { FaviconFeed } from "lib/FaviconFeed.sys.mjs";
import { actionTypes as at } from "common/Actions.mjs";
import { GlobalOverrider } from "test/unit/utils";

const FAKE_ENDPOINT = "https://foo.com/";
const FAKE_SMALLPNG_DATA_URI =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";

describe("FaviconFeed", () => {
  let feed;
  let globals;
  let sandbox;
  let clock;
  let siteIconsPref;

  beforeEach(() => {
    clock = sinon.useFakeTimers();
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    globals.set("PlacesUtils", {
      favicons: {
        setFaviconForPage: sandbox.spy(),
        getFaviconDataForPage: () => {},
        copyFavicons: sandbox.spy(),
      },
      history: {
        TRANSITIONS: {
          REDIRECT_TEMPORARY: 1,
          REDIRECT_PERMANENT: 2,
        },
      },
    });
    globals.set("NewTabUtils", {
      activityStreamProvider: { executePlacesQuery: () => Promise.resolve([]) },
    });
    siteIconsPref = true;
    sandbox
      .stub(global.Services.prefs, "getBoolPref")
      .withArgs("browser.chrome.site_icons")
      .callsFake(() => siteIconsPref);

    feed = new FaviconFeed();
    feed.store = {
      dispatch: sinon.spy(),
      getState() {
        return this.state;
      },
      state: {
        Prefs: { values: { "tippyTop.service.endpoint": FAKE_ENDPOINT } },
      },
    };
  });
  afterEach(() => {
    clock.restore();
    globals.restore();
  });

  it("should create a FaviconFeed", () => {
    assert.instanceOf(feed, FaviconFeed);
  });

  describe("#fetchIcon", () => {
    let domain;
    let url;

    beforeEach(() => {
      domain = "mozilla.org";
      url = `https://${domain}/`;
      feed.getSite = sandbox
        .stub()
        .returns(Promise.resolve({ domain, image_url: `${url}/icon.png` }));
      feed._queryForRedirects.clear();
    });

    it("should setFaviconForPage with fetching from network if no data in db", async () => {
      sandbox
        .stub(global.PlacesUtils.favicons, "getFaviconDataForPage")
        .callsArgWith(1, null, 0, null, null, 0);
      sandbox
        .stub(feed, "getFaviconDataURLFromNetwork")
        .resolves({ spec: FAKE_SMALLPNG_DATA_URI });

      await feed.fetchIcon(url);

      assert.calledOnce(global.PlacesUtils.favicons.setFaviconForPage);
      assert.calledWith(
        global.PlacesUtils.favicons.setFaviconForPage,
        sinon.match({ spec: url }),
        sinon.match({ spec: `${url}/icon.png` }),
        sinon.match({ spec: FAKE_SMALLPNG_DATA_URI })
      );
    });
    it("should setFaviconForPage with fetching from network if invalid data in db", async () => {
      sandbox
        .stub(global.PlacesUtils.favicons, "getFaviconDataForPage")
        .callsArgWith(1, { spec: FAKE_SMALLPNG_DATA_URI }, 0, null, null, 0);
      sandbox
        .stub(feed, "getFaviconDataURLFromNetwork")
        .resolves({ spec: FAKE_SMALLPNG_DATA_URI });

      await feed.fetchIcon(url);

      assert.calledOnce(global.PlacesUtils.favicons.setFaviconForPage);
      assert.calledWith(
        global.PlacesUtils.favicons.setFaviconForPage,
        sinon.match({ spec: url }),
        sinon.match({ spec: `${url}/icon.png` }),
        sinon.match({ spec: FAKE_SMALLPNG_DATA_URI })
      );
    });
    it("should setFaviconForPage if no favicon data in DB", async () => {
      sandbox
        .stub(global.PlacesUtils.favicons, "getFaviconDataForPage")
        .callsArgWith(1, null, 0, null, null, 0);
      sandbox
        .stub(feed, "getFaviconDataURLFromNetwork")
        .resolves({ spec: FAKE_SMALLPNG_DATA_URI });

      await feed.fetchIcon(url);

      assert.calledOnce(global.PlacesUtils.favicons.setFaviconForPage);
      assert.calledWith(
        global.PlacesUtils.favicons.setFaviconForPage,
        sinon.match({ spec: url }),
        sinon.match({ spec: `${url}/icon.png` }),
        sinon.match({ spec: FAKE_SMALLPNG_DATA_URI })
      );
    });
    it("should not setFaviconForPage if valid data in db", async () => {
      sandbox
        .stub(global.PlacesUtils.favicons, "getFaviconDataForPage")
        .callsArgWith(
          1,
          { spec: FAKE_SMALLPNG_DATA_URI },
          100,
          ["dummy icon data"],
          "image/png",
          16
        );

      await feed.fetchIcon(url);

      assert.notCalled(global.PlacesUtils.favicons.setFaviconForPage);
    });
    it("should NOT setFaviconForPage if site_icons pref is false", async () => {
      siteIconsPref = false;

      await feed.fetchIcon(url);

      assert.notCalled(global.PlacesUtils.favicons.setFaviconForPage);
    });
    it("should NOT setFaviconForPage if the url is NOT in the TippyTop data", async () => {
      feed.getSite = sandbox.stub().returns(Promise.resolve(null));
      await feed.fetchIcon("https://example.com");

      assert.notCalled(global.PlacesUtils.favicons.setFaviconForPage);
    });
    it("should issue a fetchIconFromRedirects if the url is NOT in the TippyTop data", async () => {
      feed.getSite = sandbox.stub().returns(Promise.resolve(null));
      sandbox.spy(global.Services.tm, "idleDispatchToMainThread");

      await feed.fetchIcon("https://example.com");

      assert.calledOnce(global.Services.tm.idleDispatchToMainThread);
    });
    it("should only issue fetchIconFromRedirects once on the same url", async () => {
      feed.getSite = sandbox.stub().returns(Promise.resolve(null));
      sandbox.spy(global.Services.tm, "idleDispatchToMainThread");

      await feed.fetchIcon("https://example.com");
      await feed.fetchIcon("https://example.com");

      assert.calledOnce(global.Services.tm.idleDispatchToMainThread);
    });
    it("should issue fetchIconFromRedirects twice on two different urls", async () => {
      feed.getSite = sandbox.stub().returns(Promise.resolve(null));
      sandbox.spy(global.Services.tm, "idleDispatchToMainThread");

      await feed.fetchIcon("https://example.com");
      await feed.fetchIcon("https://another.example.com");

      assert.calledTwice(global.Services.tm.idleDispatchToMainThread);
    });
  });

  describe("#getSite", () => {
    it("should return site data if RemoteSettings has an entry for the domain", async () => {
      const get = () =>
        Promise.resolve([{ domain: "example.com", image_url: "foo.img" }]);
      feed._tippyTop = { get };
      const site = await feed.getSite("example.com");
      assert.equal(site.domain, "example.com");
    });
    it("should return null if RemoteSettings doesn't have an entry for the domain", async () => {
      const get = () => Promise.resolve([]);
      feed._tippyTop = { get };
      const site = await feed.getSite("example.com");
      assert.isNull(site);
    });
    it("should lazy init _tippyTop", async () => {
      assert.isUndefined(feed._tippyTop);
      await feed.getSite("example.com");
      assert.ok(feed._tippyTop);
    });
  });

  describe("#onAction", () => {
    it("should fetchIcon on RICH_ICON_MISSING", async () => {
      feed.fetchIcon = sinon.spy();
      const url = "https://mozilla.org";
      feed.onAction({ type: at.RICH_ICON_MISSING, data: { url } });
      assert.calledOnce(feed.fetchIcon);
      assert.calledWith(feed.fetchIcon, url);
    });
  });

  describe("#fetchIconFromRedirects", () => {
    let domain;
    let url;
    let iconUrl;

    beforeEach(() => {
      domain = "mozilla.org";
      url = `https://${domain}/`;
      iconUrl = `${url}/icon.png`;
    });

    it("should copyFavicons if the url was redirected with a icon", async () => {
      sandbox
        .stub(global.NewTabUtils.activityStreamProvider, "executePlacesQuery")
        .resolves([
          { visit_id: 1, url: domain },
          { visit_id: 2, url },
        ]);
      sandbox
        .stub(global.PlacesUtils.favicons, "getFaviconDataForPage")
        .callsArgWith(
          1,
          { spec: iconUrl },
          100,
          ["dummy icon data"],
          "image/png",
          96
        );

      await feed.fetchIconFromRedirects(domain);

      assert.calledOnce(global.PlacesUtils.favicons.copyFavicons);
      assert.calledWith(
        global.PlacesUtils.favicons.copyFavicons,
        sinon.match({ spec: url }),
        sinon.match({ spec: domain }),
        sinon.match(global.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE)
      );
    });
    it("should NOT copyFavicons if the url doesn't have any redirect", async () => {
      sandbox
        .stub(global.NewTabUtils.activityStreamProvider, "executePlacesQuery")
        .resolves([]);

      await feed.fetchIconFromRedirects(domain);

      assert.notCalled(global.PlacesUtils.favicons.copyFavicons);
    });
    it("should NOT copyFavicons if the original url doesn't have a icon", async () => {
      sandbox
        .stub(global.NewTabUtils.activityStreamProvider, "executePlacesQuery")
        .resolves([
          { visit_id: 1, url: domain },
          { visit_id: 2, url },
        ]);
      sandbox
        .stub(global.PlacesUtils.favicons, "getFaviconDataForPage")
        .callsArgWith(1, null, 0, null, null, 0);

      await feed.fetchIconFromRedirects(domain);

      assert.notCalled(global.PlacesUtils.favicons.copyFavicons);
    });
    it("should NOT setAndFetchFaviconForPage if the original url doesn't have a rich icon", async () => {
      sandbox
        .stub(global.NewTabUtils.activityStreamProvider, "executePlacesQuery")
        .resolves([
          { visit_id: 1, url: domain },
          { visit_id: 2, url },
        ]);
      sandbox
        .stub(global.PlacesUtils.favicons, "getFaviconDataForPage")
        .callsArgWith(1, { spec: iconUrl }, 0, null, null, 16);

      await feed.fetchIconFromRedirects(domain);

      assert.notCalled(global.PlacesUtils.favicons.copyFavicons);
    });
  });
});

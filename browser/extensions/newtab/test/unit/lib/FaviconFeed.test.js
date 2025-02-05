"use strict";
import { FaviconFeed } from "lib/FaviconFeed.sys.mjs";
import { actionTypes as at } from "common/Actions.mjs";
import { GlobalOverrider } from "test/unit/utils";

describe("FaviconFeed", () => {
  let feed;
  let sandbox;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;

    feed = new FaviconFeed();
    sandbox.stub(feed.faviconProvider, "fetchIcon").resolves();
    feed.store = {
      dispatch: sinon.spy(),
    };
  });
  afterEach(() => {
    globals.restore();
  });

  it("should create a FaviconFeed", () => {
    assert.instanceOf(feed, FaviconFeed);
  });

  describe("#onAction", () => {
    it("should fetchIcon on RICH_ICON_MISSING", async () => {
      const url = "https://mozilla.org";
      feed.onAction({ type: at.RICH_ICON_MISSING, data: { url } });
      assert.calledOnce(feed.faviconProvider.fetchIcon);
      assert.calledWith(feed.faviconProvider.fetchIcon, url);
    });
  });
});

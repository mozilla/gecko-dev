import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import {PrefsFeed} from "lib/PrefsFeed.jsm";
import {PrerenderData} from "common/PrerenderData.jsm";
const {initialPrefs} = PrerenderData;

const PRERENDER_PREF_NAME = "prerender";

let overrider = new GlobalOverrider();

describe("PrefsFeed", () => {
  let feed;
  let FAKE_PREFS;
  let sandbox;
  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    FAKE_PREFS = new Map([["foo", 1], ["bar", 2]]);
    feed = new PrefsFeed(FAKE_PREFS);
    const storage = {
      getAll: sandbox.stub().resolves(),
      set: sandbox.stub().resolves(),
    };
    feed.store = {
      dispatch: sinon.spy(),
      getState() { return this.state; },
      dbStorage: {getDbTable: sandbox.stub().returns(storage)},
    };
    // Setup for tests that don't call `init`
    feed._storage = storage;
    feed._prefs = {
      get: sinon.spy(item => FAKE_PREFS.get(item)),
      set: sinon.spy((name, value) => FAKE_PREFS.set(name, value)),
      observe: sinon.spy(),
      observeBranch: sinon.spy(),
      ignore: sinon.spy(),
      ignoreBranch: sinon.spy(),
      reset: sinon.stub(),
    };
    overrider.set({PrivateBrowsingUtils: {enabled: true}});
  });
  afterEach(() => {
    overrider.restore();
    sandbox.restore();
  });
  it("should set a pref when a SET_PREF action is received", () => {
    feed.onAction(ac.SetPref("foo", 2));
    assert.calledWith(feed._prefs.set, "foo", 2);
  });
  it("should dispatch PREFS_INITIAL_VALUES on init with pref values and .isPrivateBrowsingEnabled", () => {
    feed.onAction({type: at.INIT});
    assert.calledOnce(feed.store.dispatch);
    assert.equal(feed.store.dispatch.firstCall.args[0].type, at.PREFS_INITIAL_VALUES);
    const [{data}] = feed.store.dispatch.firstCall.args;
    assert.equal(data.foo, 1);
    assert.equal(data.bar, 2);
    assert.isTrue(data.isPrivateBrowsingEnabled);
  });
  it("should add one branch observer on init", () => {
    feed.onAction({type: at.INIT});
    assert.calledOnce(feed._prefs.observeBranch);
    assert.calledWith(feed._prefs.observeBranch, feed);
  });
  it("should initialise the storage on init", () => {
    feed.init();

    assert.calledOnce(feed.store.dbStorage.getDbTable);
    assert.calledWithExactly(feed.store.dbStorage.getDbTable, "sectionPrefs");
  });
  it("should remove the branch observer on uninit", () => {
    feed.onAction({type: at.UNINIT});
    assert.calledOnce(feed._prefs.ignoreBranch);
    assert.calledWith(feed._prefs.ignoreBranch, feed);
  });
  it("should send a PREF_CHANGED action when onPrefChanged is called", () => {
    feed.onPrefChanged("foo", 2);
    assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({type: at.PREF_CHANGED, data: {name: "foo", value: 2}}));
  });
  describe("INIT prerendering", () => {
    it("should set a prerender pref on init", async () => {
      sandbox.stub(feed, "_setPrerenderPref");

      await feed.init();

      assert.calledOnce(feed._setPrerenderPref);
    });
    it("should set prerender pref to true if prefs match initial values", async () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));

      await feed._setPrerenderPref();

      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, true);
    });
    it("should set prerender pref to false if a pref does not match its initial value", async () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      FAKE_PREFS.set("showSearch", false);

      await feed._setPrerenderPref();

      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, false);
    });
    it("should set prerender pref to true if indexedDB prefs are unchanged", async () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      feed._storage.getAll.resolves([{collapsed: false}, {collapsed: false}]);

      await feed._setPrerenderPref();

      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, true);
    });
    it("should set prerender pref to false if a indexedDB pref changed value", async () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      FAKE_PREFS.set("showSearch", false);
      feed._storage.getAll.resolves([{collapsed: false}, {collapsed: true}]);

      await feed._setPrerenderPref();

      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, false);
    });
  });
  describe("indexedDB changes", () => {
    it("should call _setIndexedDBPref on UPDATE_SECTION_PREFS", () => {
      sandbox.stub(feed, "_setIndexedDBPref");

      feed.onAction({type: at.UPDATE_SECTION_PREFS, data: {}});

      assert.calledOnce(feed._setIndexedDBPref);
    });
    it("should store the pref value", async () => {
      sandbox.stub(feed, "_setPrerenderPref");
      await feed._setIndexedDBPref("topsites", "foo");

      assert.calledOnce(feed._storage.set);
      assert.calledWith(feed._storage.set, "topsites", "foo");
    });
    it("should call _setPrerenderPref", async () => {
      sandbox.stub(feed, "_setPrerenderPref");
      await feed._setIndexedDBPref("topsites", "foo");

      assert.calledOnce(feed._setPrerenderPref);
    });
    it("should catch any save errors", () => {
      const globals = new GlobalOverrider();
      globals.sandbox.spy(global.Cu, "reportError");
      feed._storage.set.throws(new Error());

      assert.doesNotThrow(() => feed._setIndexedDBPref());
      assert.calledOnce(Cu.reportError);
      globals.restore();
    });
  });
  describe("onPrefChanged prerendering", () => {
    it("should not change the prerender pref if the pref is not included in invalidatingPrefs", () => {
      feed.onPrefChanged("foo123", true);
      assert.notCalled(feed._prefs.set);
    });
    it("should set the prerender pref to false if a pref in invalidatingPrefs is changed from its original value", () => {
      sandbox.stub(feed, "_setPrerenderPref");
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));

      feed._prefs.set("showSearch", false);
      feed.onPrefChanged("showSearch", false);
      assert.calledOnce(feed._setPrerenderPref);
    });
    it("should set the prerender pref back to true if the invalidatingPrefs are changed back to their original values", () => {
      sandbox.stub(feed, "_setPrerenderPref");
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      FAKE_PREFS.set("showSearch", false);

      feed._prefs.set("showSearch", true);
      feed.onPrefChanged("showSearch", true);
      assert.calledOnce(feed._setPrerenderPref);
    });
    it("should set the prerendered pref to true", async () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      FAKE_PREFS.set("showSearch", false);
      feed._prefs.set("showSearch", true);
      feed.onPrefChanged("showSearch", true);

      await feed._setPrerenderPref();

      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, true);
    });
    it("should set the prerendered pref to false", async () => {
      Object.keys(initialPrefs).forEach(name => FAKE_PREFS.set(name, initialPrefs[name]));
      FAKE_PREFS.set("showSearch", false);
      feed._prefs.set("showSearch", false);
      feed.onPrefChanged("showSearch", false);

      await feed._setPrerenderPref();

      assert.calledWith(feed._prefs.set, PRERENDER_PREF_NAME, false);
    });
  });
  describe("migration code", () => {
    it("should migrate prefs on init", async () => {
      sandbox.stub(feed, "_migratePrefs");

      await feed.init();

      assert.calledOnce(feed._migratePrefs);
    });
    it("should migrate user set values", () => {
      FAKE_PREFS.set("collapseTopSites", true);

      feed._migratePrefs();

      assert.calledOnce(feed.store.dispatch);
      assert.calledWithExactly(feed.store.dispatch, ac.OnlyToMain({
        type: at.UPDATE_SECTION_PREFS,
        data: {id: "topsites", value: {collapsed: true}},
      }));
    });
    it("should reset any migrated prefs", () => {
      FAKE_PREFS.set("collapseTopSites", true);

      feed._migratePrefs();

      assert.calledOnce(feed._prefs.reset);
      assert.calledWithExactly(feed._prefs.reset, "collapseTopSites");
    });
  });
});

import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {
  addSnippetsSubscriber,
  SNIPPETS_UPDATE_INTERVAL_MS,
  SnippetsMap,
  SnippetsProvider,
} from "content-src/lib/snippets.js";
import {combineReducers, createStore} from "redux";
import {GlobalOverrider} from "test/unit/utils";
import {INCOMING_MESSAGE_NAME} from "content-src/lib/init-store";
import {reducers} from "common/Reducers.jsm";

describe("SnippetsMap", () => {
  let snippetsMap;
  let dispatch;
  let sandbox = sinon.createSandbox();
  let globals;
  beforeEach(() => {
    dispatch = sandbox.spy();
    snippetsMap = new SnippetsMap(dispatch);
    globals = new GlobalOverrider();
    globals.set("RPMAddMessageListener", sandbox.spy());
    globals.set("RPMRemoveMessageListener", sandbox.spy());
  });
  afterEach(async () => {
    sandbox.restore();
    globals.restore();
    await snippetsMap.clear();
  });
  describe("#set", () => {
    it("should set a value without connecting a db", async () => {
      await snippetsMap.set("foo", 123);
      assert.equal(snippetsMap.get("foo"), 123);
    });
    it("should set an item in indexedDB when connected", async () => {
      await snippetsMap.connect();
      await snippetsMap.set("foo", 123);

      // destroy the old snippetsMap, create a new one
      snippetsMap = new SnippetsMap(dispatch);
      await snippetsMap.connect();
      assert.equal(snippetsMap.get("foo"), 123);
    });
  });
  describe("#delete", () => {
    it("should delete a value without connecting a db", async () => {
      snippetsMap.set("foo", 123);
      assert.isTrue(snippetsMap.has("foo"));

      await snippetsMap.delete("foo");

      assert.isFalse(snippetsMap.has("foo"));
    });
    it("should delete an item from indexedDB when connected", async () => {
      await snippetsMap.connect();
      await snippetsMap.set("foo", 123);
      await snippetsMap.set("bar", 123);
      await snippetsMap.delete("foo");

      // destroy the old snippetsMap, create a new one
      snippetsMap = new SnippetsMap(dispatch);
      await snippetsMap.connect();
      assert.isFalse(snippetsMap.has("foo"));
      assert.isTrue(snippetsMap.has("bar"));
    });
  });
  describe("#clear", () => {
    it("should clear all items without connecting a db", async () => {
      snippetsMap.set("foo", 123);
      snippetsMap.set("bar", 123);
      assert.propertyVal(snippetsMap, "size", 2);

      await snippetsMap.clear();

      assert.propertyVal(snippetsMap, "size", 0);
    });
    it("should clear the indexedDB store when connected", async () => {
      await snippetsMap.connect();
      snippetsMap.set("foo", 123);
      snippetsMap.set("bar", 123);
      await snippetsMap.clear();

      // destroy the old snippetsMap, create a new one
      snippetsMap = new SnippetsMap(dispatch);
      await snippetsMap.connect();
      assert.propertyVal(snippetsMap, "size", 0);
    });
    it("should send SNIPPETS_BLOCKLIST_CLEARED", async () => {
      await snippetsMap.clear();

      assert.calledOnce(dispatch);
      assert.calledWithExactly(dispatch, ac.OnlyToMain({type: at.SNIPPETS_BLOCKLIST_CLEARED}));
    });
  });
  describe("#.blockList", () => {
    it("should return an empty array if blockList is not set", () => {
      assert.deepEqual(snippetsMap.blockList, []);
    });
    it("should return the blockList it is set", () => {
      snippetsMap.blockSnippetById(123);
      assert.deepEqual(snippetsMap.blockList, [123]);
    });
  });
  describe("#blockSnippetById(id)", () => {
    it("should not modify the blockList if no id was passed in", () => {
      snippetsMap.blockSnippetById(null);
      assert.deepEqual(snippetsMap.blockList, []);
    });
    it("should add new ids to the blockList", () => {
      snippetsMap.blockSnippetById(123);
      assert.deepEqual(snippetsMap.blockList, [123]);
    });
    it("should dispatch a SNIPPETS_BLOCKLIST_UPDATED event", () => {
      snippetsMap.blockSnippetById(789);

      assert.calledOnce(dispatch);
      assert.calledWith(dispatch, ac.AlsoToMain({type: at.SNIPPETS_BLOCKLIST_UPDATED, data: 789}));
    });
    it("should not add ids that are already blocked", () => {
      snippetsMap.blockSnippetById(123);
      snippetsMap.blockSnippetById(123);
      snippetsMap.blockSnippetById(456);
      assert.deepEqual(snippetsMap.blockList, [123, 456]);
    });
    it("should not call snippetsMap.set or dispatch an event for ids that are already blocked", async () => {
      snippetsMap.set("blockList", [123, 456]);
      // spy is added after so the first .set doesn't count
      sandbox.spy(snippetsMap, "set");

      await snippetsMap.blockSnippetById(123);

      assert.notCalled(dispatch);
      assert.notCalled(snippetsMap.set);
    });
  });
  describe("#showFirefoxAccounts", () => {
    it("should dispatch the a SHOW_FIREFOX_ACCOUNTS action", () => {
      snippetsMap.showFirefoxAccounts();
      assert.calledOnce(dispatch);
      assert.equal(dispatch.firstCall.args[0].type, at.SHOW_FIREFOX_ACCOUNTS);
    });
  });
  describe("#getTotalBookmarksCount", () => {
    it("should dispatch a TOTAL_BOOKMARKS_REQUEST and resolve with the right data", async () => {
      const bookmarksPromise = snippetsMap.getTotalBookmarksCount();

      assert.calledOnce(dispatch);
      assert.equal(dispatch.firstCall.args[0].type, at.TOTAL_BOOKMARKS_REQUEST);
      assert.calledWith(global.RPMAddMessageListener, INCOMING_MESSAGE_NAME);

      // Call listener
      const [, listener] = global.RPMAddMessageListener.firstCall.args;
      listener({data: {type: at.TOTAL_BOOKMARKS_RESPONSE, data: 42}});

      const result = await bookmarksPromise;

      assert.equal(result, 42);
      assert.calledWith(global.RPMRemoveMessageListener, INCOMING_MESSAGE_NAME, listener);
    });
  });
  describe("#getAddonsInfo", () => {
    it("should dispatch a ADDONS_INFO_REQUEST and resolve with the right data", async () => {
      const addonsPromise = snippetsMap.getAddonsInfo();

      assert.calledOnce(dispatch);
      assert.equal(dispatch.firstCall.args[0].type, at.ADDONS_INFO_REQUEST);
      assert.calledWith(global.RPMAddMessageListener, INCOMING_MESSAGE_NAME);

      // Call listener
      const [, listener] = global.RPMAddMessageListener.firstCall.args;
      const addonsInfo = {isFullData: true, addons: {foo: {}}};
      listener({data: {type: at.ADDONS_INFO_RESPONSE, data: addonsInfo}});

      const result = await addonsPromise;

      assert.equal(result, addonsInfo);
      assert.calledWith(global.RPMRemoveMessageListener, INCOMING_MESSAGE_NAME, listener);
    });
  });
});

describe("SnippetsProvider", () => {
  let sandbox;
  let snippets;
  let globals;
  let dispatch;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    sandbox.stub(window, "fetch").returns(Promise.resolve(""));
    globals = new GlobalOverrider();
    globals.set("RPMAddMessageListener", sandbox.spy());
    globals.set("RPMRemoveMessageListener", sandbox.spy());
    dispatch = sandbox.stub();
  });
  afterEach(async () => {
    if (global.gSnippetsMap) {
      await global.gSnippetsMap.clear();
    }
    delete global.gSnippetsMap;
    globals.restore();
    sandbox.restore();
  });
  it("should create a gSnippetsMap with a dispatch function", () => {
    snippets = new SnippetsProvider(dispatch);
    assert.instanceOf(global.gSnippetsMap, SnippetsMap);
    assert.equal(global.gSnippetsMap._dispatch, dispatch);
  });
  describe("#init(options)", () => {
    beforeEach(() => {
      snippets = new SnippetsProvider(dispatch);
      sandbox.stub(snippets, "_refreshSnippets").returns(Promise.resolve());
      sandbox.stub(snippets, "_showRemoteSnippets");
    });
    it("should connect to the database by default", () => {
      sandbox.stub(global.gSnippetsMap, "connect").returns(Promise.resolve());

      snippets.init();

      assert.calledOnce(global.gSnippetsMap.connect);
    });
    it("should log connection errors without throwing", async () => {
      sandbox.stub(global.console, "error");
      sandbox.stub(global.gSnippetsMap, "connect").returns(() => { throw new Error(); });
      await snippets.init();

      assert.calledOnce(global.console.error);
    });
    it("should not call connect if options.connect is false", async () => {
      sandbox.stub(global.gSnippetsMap, "connect").returns(Promise.resolve());

      await snippets.init({connect: false});

      assert.notCalled(global.gSnippetsMap.connect);
    });
    it("should call _refreshSnippets and _showRemoteSnippets", async () => {
      await snippets.init({connect: false});

      assert.calledOnce(snippets._refreshSnippets);
      assert.calledOnce(snippets._showRemoteSnippets);
    });
    it("should not throw if _showRemoteSnippets throws an error", async () => {
      snippets._showRemoteSnippets.callsFake(() => { throw new Error("error"); });
      await snippets.init({connect: false});
    });
    it("should set each item in .appData in gSnippetsMap as appData.{item}", async () => {
      await snippets.init({connect: false, appData: {foo: 123, bar: "hello"}});
      assert.equal(snippets.snippetsMap.get("appData.foo"), 123);
      assert.equal(snippets.snippetsMap.get("appData.bar"), "hello");
    });
    it("should dispatch a Snippets:Enabled DOM event", async () => {
      const spy = sinon.spy();
      window.addEventListener("Snippets:Enabled", spy);
      await snippets.init({connect: false});
      assert.calledOnce(spy);
      window.removeEventListener("Snippets:Enabled", spy);
    });
    it("should add a message listener for incoming messages", async () => {
      await snippets.init({connect: false});
      assert.calledWith(global.RPMAddMessageListener, INCOMING_MESSAGE_NAME, snippets._onAction);
    });
    it("should set the blocklist based on init options", async () => {
      const blockList = [1, 2, 3];
      await snippets.init({appData: {blockList}});

      assert.equal(snippets.snippetsMap.get("blockList"), blockList);
    });
  });
  describe("#uninit", () => {
    let fakeEl;
    beforeEach(() => {
      fakeEl = {style: {}};
      sandbox.stub(global.document, "getElementById").returns(fakeEl);
    });
    it("should dispatch a Snippets:Disabled DOM event", () => {
      const spy = sinon.spy();
      window.addEventListener("Snippets:Disabled", spy);
      snippets = new SnippetsProvider(dispatch);
      snippets.uninit();
      assert.calledOnce(spy);
      window.removeEventListener("Snippets:Disabled", spy);
    });
    it("should remove the message listener for incoming messages", () => {
      snippets = new SnippetsProvider(dispatch);
      snippets.uninit();
      assert.calledWith(global.RPMRemoveMessageListener, INCOMING_MESSAGE_NAME, snippets._onAction);
    });
  });
  describe("#_refreshSnippets", () => {
    let clock;
    beforeEach(() => {
      clock = sinon.useFakeTimers();
      snippets = new SnippetsProvider(dispatch);
      global.gSnippetsMap.set("snippets-cached-version", 4);
    });
    afterEach(() => {
      clock.restore();
    });
    it("should clear gSnippetsMap if the cached version is different than the current version or missing", async () => {
      sandbox.spy(global.gSnippetsMap, "clear");
      await snippets.init({connect: false, appData: {version: 5, snippetsURL: "foo.com"}});
      assert.calledOnce(global.gSnippetsMap.clear);

      global.gSnippetsMap.clear.resetHistory();
      global.gSnippetsMap.delete("snippets-cached-version");
      await snippets.init({connect: false, appData: {version: 5, snippetsURL: "foo.com"}});
      assert.calledOnce(global.gSnippetsMap.clear);
    });
    it("should not clear gSnippetsMap if the cached version the same as the current version", async () => {
      sandbox.spy(global.gSnippetsMap, "clear");
      await snippets.init({connect: false, appData: {version: 4, snippetsURL: "foo.com"}});
      assert.notCalled(global.gSnippetsMap.clear);
    });
    it("should fetch new data if no last update time was cached", async () => {
      await snippets.init({connect: false, appData: {version: 4, snippetsURL: "foo.com"}});
      assert.calledOnce(global.fetch);
    });
    it("should fetch new data if it has been at least SNIPPETS_UPDATE_INTERVAL_MS since the last update", async () => {
      global.gSnippetsMap.set("snippets-last-update", Date.now());
      clock.tick(SNIPPETS_UPDATE_INTERVAL_MS + 1);

      await snippets.init({connect: false, appData: {version: 4, snippetsURL: "foo.com"}});

      assert.calledOnce(global.fetch);
    });
    it("should NOT fetch new data if it has been less than 4 hours since the last update", async () => {
      global.gSnippetsMap.set("snippets-last-update", Date.now());

      clock.tick(5);

      await snippets.init({connect: false, appData: {version: 4, snippetsURL: "foo.com"}});

      assert.notCalled(global.fetch);
    });
    it("should set payload, last-update, and cached-version after fetching", async () => {
      clock.tick(7);
      global.fetch.returns(Promise.resolve({status: 200, text: () => Promise.resolve("foo123")}));

      await snippets.init({connect: false, appData: {version: 5, snippetsURL: "foo.com"}});

      assert.equal(global.gSnippetsMap.get("snippets"), "foo123");
      assert.equal(global.gSnippetsMap.get("snippets-last-update"), 7);
      assert.equal(global.gSnippetsMap.get("snippets-cached-version"), 5);
    });
    it("should catch fetch errors gracefully", async () => {
      const testError = new Error({status: 400});
      sandbox.stub(global.console, "error");
      global.fetch.returns(Promise.reject(testError));

      await snippets.init({connect: false, appData: {version: 5, snippetsURL: "foo.com"}});

      assert.calledWith(global.console.error, testError);
    });
  });
  describe("#_showRemoteSnippets", () => {
    beforeEach(() => {
      snippets = new SnippetsProvider(dispatch);
      sandbox.stub(snippets, "_refreshSnippets").returns(Promise.resolve());
      let fakeEl = {style: {}, getElementsByTagName() { return [{parentNode: {replaceChild() {}}}]; }};
      sandbox.stub(global.document, "getElementById").returns(fakeEl);
    });
    it("should log error if no snippets element exists", async () => {
      global.gSnippetsMap.set("snippets", "foo123");
      global.document.getElementById.returns(null);
      sandbox.stub(global.console, "error");
      await snippets.init({connect: false});

      const [error] = global.console.error.firstCall.args;
      assert.match(error.message, "No element was found");
    });
    it("should log error if no payload is found", async () => {
      sandbox.stub(global.console, "error");
      global.gSnippetsMap.set("snippets", "");
      await snippets.init({connect: false});

      const [error] = global.console.error.firstCall.args;
      assert.match(error.message, "No remote snippets were found");
    });
    it("should log error if the payload is not a string", async () => {
      sandbox.stub(global.console, "error");
      global.gSnippetsMap.set("snippets", true);
      await snippets.init({connect: false});

      const [error] = global.console.error.firstCall.args;
      assert.match(error.message, "Snippet payload was incorrectly formatted");
    });
  });
  describe("blocking", () => {
    let containerEl;
    beforeEach(() => {
      snippets = new SnippetsProvider(dispatch);
      containerEl = {style: {}};
      sandbox.stub(global.document, "getElementById").returns(containerEl);
    });
    it("should set the blockList and hide the element if an incoming SNIPPET_BLOCKED message is received", async () => {
      assert.deepEqual(global.gSnippetsMap.blockList, []);
      await snippets.init({connect: false});
      const action = {type: at.SNIPPET_BLOCKED, data: "foo"};

      snippets._onAction({name: INCOMING_MESSAGE_NAME, data: action});

      assert.deepEqual(global.gSnippetsMap.blockList, ["foo"]);
      assert.calledWith(global.document.getElementById, "snippets-container");
      assert.equal(containerEl.style.display, "none");
    });
    it("should not do anything if the incoming message is not a SNIPPET_BLOCKED action", async () => {
      await snippets.init({connect: false});
      const prevValue = containerEl.style.display;
      const action = {type: "FOO", data: ["foo"]};

      snippets._onAction({name: INCOMING_MESSAGE_NAME, data: action});
      assert.deepEqual(global.gSnippetsMap.blockList, []);
      assert.equal(containerEl.style.display, prevValue);
    });
  });
});

describe("addSnippetsSubscriber", () => {
  let store;
  let sandbox;
  let snippets;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    store = createStore(combineReducers(reducers));
    sandbox.spy(store, "subscribe");
    ({snippets} = addSnippetsSubscriber(store));

    sandbox.stub(snippets, "init").callsFake(() => {
      snippets.initialized = true;
    }).resolves();
    sandbox.stub(snippets, "uninit").callsFake(() => {
      snippets.initialized = false;
    });
  });
  afterEach(async () => {
    sandbox.restore();
    if (global.gSnippetsMap) {
      await global.gSnippetsMap.clear();
    }
    delete global.gSnippetsMap;
  });
  // it should initialize if:
  // as router and snippets feed are both on.
  // disable snippets is false

  function setConditions({
    snippetsFeedInitialized = true,
    asrIntialized = true,
    allowLegacySnippets = true,
    forceDisablePrefOn = false,
    userPrefOn = true,
  } = {}) {
    [
      // The snippets feed should be initialized;
      snippetsFeedInitialized && {type: at.SNIPPETS_DATA, data: {}},
      // ASR should be initialized;
      asrIntialized && {type: at.AS_ROUTER_INITIALIZED, data: {}},
      // Allow legacy snippets pref should be true in ASRouterPreferences
      {type: at.AS_ROUTER_PREF_CHANGED, data: {allowLegacySnippets}},
      // Force disable pref is on
      {type: at.PREF_CHANGED, data: {name: "disableSnippets", value: forceDisablePrefOn}},
      // Is the user setting for snippets on?
      {type: at.PREF_CHANGED, data: {name: "feeds.snippets", value: userPrefOn}},
    ].filter(a => a).forEach(action => store.dispatch(action));
  }

  it("should initialize when snippets feed when all conditions are met (seee default for setConditions)", () => {
    setConditions();
    assert.calledOnce(snippets.init);
  });
  it("should not initialize if snippets feed has not been initialized", () => {
    setConditions({snippetsFeedInitialized: false});

    assert.calledOnce(store.subscribe);
    assert.notCalled(snippets.init);
  });
  it("should not initialize if ASR has not been initialized", () => {
    setConditions({asrIntialized: false});

    assert.calledOnce(store.subscribe);
    assert.notCalled(snippets.init);
  });

  it("should not initialize if disableSnippets pref is true", () => {
    setConditions({forceDisablePrefOn: true});

    assert.calledOnce(store.subscribe);
    assert.notCalled(snippets.init);
  });
  it("should uninitialize if disableSnippets pref has been changed to true", async () => {
    setConditions();
    snippets.initialized = true;
    setConditions({forceDisablePrefOn: true});
    assert.calledOnce(snippets.uninit);
  });

  it("should not initialize if user pref is false", () => {
    setConditions({userPrefOn: false});
    store.dispatch({type: at.SNIPPETS_DATA, data: {}});
    assert.notCalled(snippets.init);
  });
  it("should uninitialize if the user pref has been changed to false", async () => {
    setConditions();
    snippets.initialized = true;
    setConditions({userPrefOn: false});
    assert.calledOnce(snippets.uninit);
  });

  it("should not initialize if allowLegacySnippets is false", () => {
    setConditions({allowLegacySnippets: false});

    assert.calledOnce(store.subscribe);
    assert.notCalled(snippets.init);
  });
  it("should uninitialize if allowLegacySnippets is changed to false", async () => {
    setConditions();
    snippets.initialized = true;
    setConditions({allowLegacySnippets: false});
    assert.calledOnce(snippets.uninit);
  });
});

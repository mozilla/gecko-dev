/* global Services */
import {
  AboutPreferences,
  PREFERENCES_LOADED_EVENT,
} from "lib/AboutPreferences.sys.mjs";
import { actionTypes as at, actionCreators as ac } from "common/Actions.mjs";
import { GlobalOverrider } from "test/unit/utils";

describe("AboutPreferences Feed", () => {
  let globals;
  let sandbox;
  let Sections;
  let DiscoveryStream;
  let instance;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    Sections = [];
    DiscoveryStream = { config: { enabled: false } };
    instance = new AboutPreferences();
    instance.store = {
      dispatch: sandbox.stub(),
      getState: () => ({ Sections, DiscoveryStream }),
    };
    globals.set("NimbusFeatures", {
      newtab: { getAllVariables: sandbox.stub() },
    });
  });
  afterEach(() => {
    globals.restore();
  });

  describe("#onAction", () => {
    it("should call .init() on an INIT action", () => {
      const stub = sandbox.stub(instance, "init");

      instance.onAction({ type: at.INIT });

      assert.calledOnce(stub);
    });
    it("should call .uninit() on an UNINIT action", () => {
      const stub = sandbox.stub(instance, "uninit");

      instance.onAction({ type: at.UNINIT });

      assert.calledOnce(stub);
    });
    it("should call .openPreferences on SETTINGS_OPEN", () => {
      const action = {
        type: at.SETTINGS_OPEN,
        _target: { browser: { ownerGlobal: { openPreferences: sinon.spy() } } },
      };
      instance.onAction(action);
      assert.calledOnce(action._target.browser.ownerGlobal.openPreferences);
    });
    it("should call .BrowserAddonUI.openAddonsMgr with the extension id on OPEN_WEBEXT_SETTINGS", () => {
      const action = {
        type: at.OPEN_WEBEXT_SETTINGS,
        data: "foo",
        _target: {
          browser: {
            ownerGlobal: {
              BrowserAddonUI: { openAddonsMgr: sinon.spy() },
            },
          },
        },
      };
      instance.onAction(action);
      assert.calledWith(
        action._target.browser.ownerGlobal.BrowserAddonUI.openAddonsMgr,
        "addons://detail/foo"
      );
    });
  });

  describe("#observe", () => {
    let renderPreferenceSection;
    let toggleRestoreDefaults;

    beforeEach(() => {
      // Stub out All The Things
      renderPreferenceSection = sandbox.stub(
        instance,
        "renderPreferenceSection"
      );
      toggleRestoreDefaults = sandbox.stub(instance, "toggleRestoreDefaults");
    });

    it("should watch for about:preferences loading", () => {
      sandbox.stub(Services.obs, "addObserver");

      instance.init();

      assert.calledOnce(Services.obs.addObserver);
      assert.calledWith(
        Services.obs.addObserver,
        instance,
        PREFERENCES_LOADED_EVENT
      );
    });
    it("should stop watching on uninit", () => {
      sandbox.stub(Services.obs, "removeObserver");

      instance.uninit();

      assert.calledOnce(Services.obs.removeObserver);
      assert.calledWith(
        Services.obs.removeObserver,
        instance,
        PREFERENCES_LOADED_EVENT
      );
    });
    it("should try to render on event", async () => {
      Sections.push({
        rowsPref: "row_pref",
        maxRows: 3,
        pref: { descString: "foo" },
        learnMore: { link: "https://foo.com" },
        id: "topstories",
      });

      Sections.push({
        rowsPref: "row_pref",
        maxRows: 3,
        pref: { descString: "foo" },
        learnMore: { link: "https://foo.com" },
        id: "highlights",
      });

      await instance.observe(window, PREFERENCES_LOADED_EVENT);

      // Render all the prefs
      assert.callCount(renderPreferenceSection, 6);

      // Show or hide the "Restore defaults" button depending on prefs
      assert.calledOnce(toggleRestoreDefaults);
    });
  });

  describe("#renderPreferenceSection", () => {
    let node;
    let Preferences;
    let document;

    beforeEach(() => {
      node = {
        appendChild: sandbox.stub().returnsArg(0),
        addEventListener: sandbox.stub(),
        classList: { add: sandbox.stub(), remove: sandbox.stub() },
        cloneNode: sandbox.stub().returnsThis(),
        insertAdjacentElement: sandbox.stub().returnsArg(1),
        setAttribute: sandbox.stub(),
        remove: sandbox.stub(),
        style: {},
      };
      document = {
        createXULElement: sandbox.stub().returns(node),
        l10n: {
          setAttributes(el, id, args) {
            el.setAttribute("data-l10n-id", id);
            el.setAttribute("data-l10n-args", JSON.stringify(args));
          },
        },
        createProcessingInstruction: sandbox.stub(),
        createElementNS: sandbox.stub().callsFake(() => node),
        getElementById: sandbox.stub().returns(node),
        insertBefore: sandbox.stub().returnsArg(0),
        querySelector: sandbox.stub().returns({ appendChild: sandbox.stub() }),
      };
      Preferences = {
        add: sandbox.stub(),
        get: sandbox.stub().returns({
          on: sandbox.stub(),
        }),
      };
    });

    describe("#linkPref", () => {
      it("should add a pref to the global", () => {
        const sectionData = { pref: { feed: "feed" } };
        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.calledOnce(Preferences.add);
      });

      it("should skip adding if not shown", () => {
        const sectionData = { shouldHidePref: true };
        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.notCalled(Preferences.add);
      });
    });

    describe("title line", () => {
      it("should render a title", () => {
        const titleString = "the_title";
        const sectionData = { pref: { titleString } };
        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.calledWith(node.setAttribute, "data-l10n-id", titleString);
      });
    });

    describe("top stories", () => {
      const href = "https://disclaimer/";
      const eventSource = "https://disclaimer/";
      let sectionData;

      beforeEach(() => {
        sectionData = {
          id: "topstories",
          pref: { feed: "feed", learnMore: { link: { href } } },
          eventSource,
        };
      });

      it("should setup a user event for top stories eventSource", () => {
        sinon.spy(instance, "setupUserEvent");
        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.calledWith(node.addEventListener, "command");
        assert.calledWith(instance.setupUserEvent, node, eventSource);
      });

      it("should setup a user event for top stories nested pref eventSource", () => {
        sinon.spy(instance, "setupUserEvent");
        const section = {
          id: "topstories",
          pref: {
            feed: "feed",
            learnMore: { link: { href } },
            nestedPrefs: [
              {
                name: "showSponsored",
                titleString:
                  "home-prefs-recommended-by-option-sponsored-stories",
                icon: "icon-info",
                eventSource: "POCKET_SPOCS",
              },
            ],
          },
        };
        instance.renderPreferenceSection(section, document, Preferences);

        assert.calledWith(node.addEventListener, "command");
        assert.calledWith(instance.setupUserEvent, node, "POCKET_SPOCS");
      });

      it("should fire store dispatch with onCommand", () => {
        const element = {
          addEventListener: (command, action) => {
            // Trigger the action right away because we only care about testing the action here.
            action({ target: { checked: true } });
          },
        };
        instance.setupUserEvent(element, eventSource);
        assert.calledWith(
          instance.store.dispatch,
          ac.UserEvent({
            event: "PREF_CHANGED",
            source: eventSource,
            value: { menu_source: "ABOUT_PREFERENCES", status: true },
          })
        );
      });

      // The Weather pref now has a link to learn more, other prefs such as Top Stories don't any more
      it("should add a link for weather", () => {
        const section = {
          id: "weather",
          pref: { feed: "feed", learnMore: { link: { href } } },
          eventSource,
        };

        instance.renderPreferenceSection(section, document, Preferences);

        assert.calledWith(node.setAttribute, "href", href);
      });
    });

    describe("description line", () => {
      it("should render a description", () => {
        const descString = "the_desc";
        const sectionData = { pref: { descString } };

        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.calledWith(node.setAttribute, "data-l10n-id", descString);
      });

      it("should render rows dropdown with appropriate number", () => {
        const sectionData = {
          rowsPref: "row_pref",
          maxRows: 3,
          pref: { descString: "foo" },
        };

        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.calledWith(node.setAttribute, "value", 1);
        assert.calledWith(node.setAttribute, "value", 2);
        assert.calledWith(node.setAttribute, "value", 3);
      });
    });
    describe("nested prefs", () => {
      const titleString = "im_nested";
      let sectionData;

      beforeEach(() => {
        sectionData = { pref: { nestedPrefs: [{ titleString }] } };
      });

      it("should render a nested pref", () => {
        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.calledWith(node.setAttribute, "data-l10n-id", titleString);
      });

      it("should set node hidden to true", () => {
        sectionData.pref.nestedPrefs[0].hidden = true;

        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.isTrue(node.hidden);
      });
      it("should add a change event", () => {
        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.calledOnce(Preferences.get().on);
        assert.calledWith(Preferences.get().on, "change");
      });

      it("should default node disabled to false", async () => {
        Preferences.get = sandbox.stub().returns({
          on: sandbox.stub(),
          _value: true,
        });

        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.isFalse(node.disabled);
      });
      it("should default node disabled to true", async () => {
        instance.renderPreferenceSection(sectionData, document, Preferences);

        assert.isTrue(node.disabled);
      });
      it("should set node disabled to true", async () => {
        const pref = {
          on: sandbox.stub(),
          _value: true,
        };
        Preferences.get = sandbox.stub().returns(pref);

        instance.renderPreferenceSection(sectionData, document, Preferences);
        pref._value = !pref._value;
        await Preferences.get().on.firstCall.args[1]();

        assert.isTrue(node.disabled);
      });
      it("should set node disabled to false", async () => {
        const pref = {
          on: sandbox.stub(),
          _value: false,
        };
        Preferences.get = sandbox.stub().returns(pref);

        instance.renderPreferenceSection(sectionData, document, Preferences);
        pref._value = !pref._value;
        await Preferences.get().on.firstCall.args[1]();

        assert.isFalse(node.disabled);
      });
    });
  });

  describe("#toggleRestoreDefaults", () => {
    it("should call toggleRestoreDefaultsBtn", async () => {
      let gHomePane;
      gHomePane = { toggleRestoreDefaultsBtn: sandbox.stub() };

      await instance.toggleRestoreDefaults(gHomePane);

      assert.calledOnce(gHomePane.toggleRestoreDefaultsBtn);
    });
  });

  describe("#getString", () => {
    it("should not fail if titleString is not provided", () => {
      const emptyPref = {};

      const returnString = instance.getString(emptyPref);
      assert.equal(returnString, undefined);
    });

    it("should return the string id if titleString is just a string", () => {
      const titleString = "foo";

      const returnString = instance.getString(titleString);
      assert.equal(returnString, titleString);
    });

    it("should set id and args if titleString is an object with id and values", () => {
      const titleString = { id: "foo", values: { provider: "bar" } };

      const returnString = instance.getString(titleString);
      assert.equal(returnString, titleString.id);
    });
  });
});

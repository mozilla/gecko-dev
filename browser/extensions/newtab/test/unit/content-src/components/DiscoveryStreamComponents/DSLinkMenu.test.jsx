import { mount } from "enzyme";
import { DSLinkMenu } from "content-src/components/DiscoveryStreamComponents/DSLinkMenu/DSLinkMenu";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import React from "react";
import { Provider } from "react-redux";
import { combineReducers, createStore } from "redux";
import { INITIAL_STATE, reducers } from "common/Reducers.sys.mjs";

describe("<DSLinkMenu>", () => {
  let wrapper;
  let store;

  describe("DS link menu actions", () => {
    beforeEach(() => {
      store = createStore(combineReducers(reducers), INITIAL_STATE);
      wrapper = mount(
        <Provider store={store}>
          <DSLinkMenu />
        </Provider>
      );
    });

    afterEach(() => {
      wrapper.unmount();
    });

    it("should parse args for fluent correctly ", () => {
      const title = '"fluent"';
      wrapper = mount(
        <Provider store={store}>
          <DSLinkMenu title={title} />
        </Provider>
      );

      const button = wrapper.find(
        "button[data-l10n-id='newtab-menu-content-tooltip']"
      );
      assert.equal(button.prop("data-l10n-args"), JSON.stringify({ title }));
    });
  });

  describe("DS context menu options", () => {
    const ValidDSLinkMenuProps = {
      site: {},
      card_type: "organic",
    };

    beforeEach(() => {
      wrapper = mount(
        <Provider store={store}>
          <DSLinkMenu {...ValidDSLinkMenuProps} />
        </Provider>
      );
    });

    afterEach(() => {
      wrapper.unmount();
    });

    it("should render a context menu button", () => {
      assert.ok(wrapper.exists());
      assert.ok(
        wrapper.find(ContextMenuButton).exists(),
        "context menu button exists"
      );
    });

    it("should render LinkMenu when context menu button is clicked", () => {
      let button = wrapper.find(ContextMenuButton);
      button.simulate("click", { preventDefault: () => {} });
      assert.equal(wrapper.find(LinkMenu).length, 1);
    });

    it("should pass dispatch, onShow, site, options, shouldSendImpressionStats, source and index to LinkMenu", () => {
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      [
        "dispatch",
        "onShow",
        "site",
        "index",
        "options",
        "source",
        "shouldSendImpressionStats",
      ].forEach(prop => assert.property(linkMenuProps, prop));
    });

    it("should pass through the correct menu options to LinkMenu for recommended stories", () => {
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "CheckBookmark",
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
      ]);
    });

    it("should pass through ReportContent as a link menu option when section is defined", () => {
      wrapper = mount(
        <Provider store={store}>
          <DSLinkMenu {...ValidDSLinkMenuProps} section="abc" />
        </Provider>
      );

      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "CheckBookmark",
        "Separator",
        "OpenInNewWindow",
        "OpenInPrivateWindow",
        "Separator",
        "BlockUrl",
        "ReportContent",
      ]);
    });

    it("should pass through the correct menu options to LinkMenu for SPOCs", () => {
      wrapper = mount(
        <Provider store={store}>
          <DSLinkMenu {...ValidDSLinkMenuProps} card_type="spoc" />
        </Provider>
      );
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "BlockUrl",
        "ManageSponsoredContent",
        "OurSponsorsAndYourPrivacy",
      ]);
    });

    it("should pass through the correct menu options to LinkMenu for SPOCs when ReportAds enabled", () => {
      const stateWithReporting = {
        ...INITIAL_STATE,
        Prefs: {
          ...INITIAL_STATE.Prefs,
          values: {
            ...INITIAL_STATE.Prefs.values,
            "discoverystream.reportAds.enabled": true,
          },
        },
      };

      store = createStore(combineReducers(reducers), stateWithReporting);

      wrapper = mount(
        <Provider store={store}>
          <DSLinkMenu
            {...ValidDSLinkMenuProps}
            card_type="spoc"
            shim={{ report: {} }}
          />
        </Provider>
      );
      wrapper
        .find(ContextMenuButton)
        .simulate("click", { preventDefault: () => {} });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "BlockUrl",
        "ReportAd",
        "ManageSponsoredContent",
        "OurSponsorsAndYourPrivacy",
      ]);
    });
  });
});

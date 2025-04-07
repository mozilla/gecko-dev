import { shallow } from "enzyme";
import { AdBannerContextMenu } from "content-src/components/DiscoveryStreamComponents/AdBannerContextMenu/AdBannerContextMenu";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import React from "react";

describe("<AdBannerContextMenu>", () => {
  let wrapper;

  describe("Ad banner context menu options", () => {
    const props = {
      spoc: { url: "https://www.test.com/", shim: "aaabbbcccddd" },
      position: 1,
      type: "billboard",
      prefs: {},
    };

    beforeEach(() => {
      wrapper = shallow(<AdBannerContextMenu {...props} />);
    });

    it("should render a context menu button", () => {
      assert.ok(wrapper.exists());
      assert.ok(
        wrapper.find("moz-button").exists(),
        "context menu button exists"
      );
    });

    it("should render LinkMenu when context menu button is clicked", () => {
      let button = wrapper.find("moz-button");
      button.simulate("click", {
        preventDefault: () => {},
      });
      assert.equal(wrapper.find(LinkMenu).length, 1);
    });

    it("should pass props to LinkMenu", () => {
      wrapper.find("moz-button").simulate("click", {
        preventDefault: () => {},
      });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      [
        "onUpdate",
        "dispatch",
        "options",
        "shouldSendImpressionStats",
        "userEvent",
        "site",
        "index",
        "source",
      ].forEach(prop => assert.property(linkMenuProps, prop));
    });

    it("should pass through the correct menu options to LinkMenu for ad banners with reporting INCLUDED", () => {
      const propsWithReporting = {
        ...props,
        showAdReporting: true,
      };
      wrapper = shallow(<AdBannerContextMenu {...propsWithReporting} />);
      wrapper.find("moz-button").simulate("click", {
        preventDefault: () => {},
      });
      const linkMenuProps = wrapper.find(LinkMenu).props();

      const linkMenuOptions = [
        "BlockAdUrl",
        "ReportAd",
        "ManageSponsoredContent",
        "OurSponsorsAndYourPrivacy",
      ];

      assert.deepEqual(linkMenuProps.options, linkMenuOptions);
    });

    it("should pass through correct menu options to LinkMenu for ad banner with reporting EXCLUDED", () => {
      const propsWithoutReporting = {
        ...props,
        showAdReporting: false,
      };

      wrapper = shallow(<AdBannerContextMenu {...propsWithoutReporting} />);
      wrapper.find("moz-button").simulate("click", {
        preventDefault: () => {},
      });
      const linkMenuProps = wrapper.find(LinkMenu).props();

      const linkMenuOptions = [
        "BlockAdUrl",
        "ManageSponsoredContent",
        "OurSponsorsAndYourPrivacy",
      ];

      assert.deepEqual(linkMenuProps.options, linkMenuOptions);
    });
  });
});

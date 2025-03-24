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

    it("should pass through the correct menu options to LinkMenu for ad banners", () => {
      wrapper.find("moz-button").simulate("click", {
        preventDefault: () => {},
      });
      const linkMenuProps = wrapper.find(LinkMenu).props();
      assert.deepEqual(linkMenuProps.options, [
        "BlockAdUrl",
        "ManageSponsoredContent",
        "OurSponsorsAndYourPrivacy",
      ]);
    });
  });
});

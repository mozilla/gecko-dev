import React from "react";
import { mount } from "enzyme";
import { AddonsPicker } from "content-src/components/AddonsPicker";
import { AboutWelcomeUtils } from "content-src/lib/aboutwelcome-utils.mjs";

describe("AddonsPicker component", () => {
  let sandbox;
  let wrapper;

  const ADDON_DATA = {
    id: "addon",
    name: "Addon Name",
    description: "Addon Description",
    icon: "icon-url.png",
    type: "extension",
    author: {
      id: "author",
      name: "Author Name",
    },
    action: {
      type: "INSTALL_ADDON_FROM_URL",
      data: { url: "https://example.com/addon" },
    },
    source_id: "addon1_source",
  };

  const ADDON_CONTENT = {
    tiles: {
      data: [ADDON_DATA],
    },
  };

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    sandbox.stub(AboutWelcomeUtils, "handleUserAction");
    sandbox.stub(AboutWelcomeUtils, "sendActionTelemetry");
  });

  afterEach(() => {
    sandbox.restore();
    if (wrapper && wrapper.unmount) {
      wrapper.unmount();
    }
  });

  describe("Centered layout", () => {
    beforeEach(() => {
      wrapper = mount(
        <AddonsPicker
          content={ADDON_CONTENT}
          installedAddons={[]}
          message_id="test_message"
        />
      );
    });

    it("should render the component when content is provided", () => {
      assert.ok(wrapper.exists());
    });

    it("should not render the component when no content is provided", () => {
      wrapper.setProps({ content: null });
      assert.ok(wrapper.isEmptyRender());
    });

    it("should not render addon rows container", () => {
      const addonContainer = wrapper.find(".addon-container").at(0);
      assert.equal(addonContainer.find(".addon-rows-container").length, 0);
    });

    it("should render addon title and description", () => {
      const addonDetails = wrapper.find(".addon-details").at(0);
      assert.equal(addonDetails.find(".addon-title").length, 1);
      assert.equal(addonDetails.find(".addon-description").length, 1);
    });
  });

  describe("Split layout", () => {
    beforeEach(() => {
      wrapper = mount(
        <AddonsPicker
          content={ADDON_CONTENT}
          installedAddons={[]}
          message_id="test_message"
          layout="split"
        />
      );
    });

    it("should render the component in split layout", () => {
      assert.ok(wrapper.exists());
      assert.equal(wrapper.find(".addon-rows-container").length, 1);
    });

    it("should render addon title and author in the first row", () => {
      const firstRow = wrapper.find(".addon-row").at(0);
      assert.equal(firstRow.find(".addon-author-details").length, 1);
      assert.equal(firstRow.find(".addon-title").length, 1);
      assert.equal(firstRow.find(".addon-author").length, 1);
    });

    it("should render install button in the first row", () => {
      const firstRow = wrapper.find(".addon-row").at(0);
      assert.equal(firstRow.find("InstallButton").length, 1);
    });

    it("should render description in the second row", () => {
      const secondRow = wrapper.find(".addon-row").at(1);
      assert.equal(secondRow.find(".addon-description").length, 1);
    });

    it("should handle author link click correctly", () => {
      const authorLink = wrapper.find(".author-link");
      authorLink.simulate("click", { stopPropagation: sandbox.stub() });

      assert.calledOnce(AboutWelcomeUtils.handleUserAction);
      assert.calledWith(AboutWelcomeUtils.handleUserAction, {
        type: "OPEN_URL",
        data: {
          args: `https://addons.mozilla.org/firefox/user/${ADDON_DATA.author.id}/`,
          where: "tab",
        },
      });
    });
  });
});

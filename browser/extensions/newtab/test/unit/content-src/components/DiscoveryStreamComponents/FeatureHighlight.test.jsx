import { FeatureHighlight } from "content-src/components/DiscoveryStreamComponents/FeatureHighlight/FeatureHighlight";
import { SponsoredContentHighlight } from "content-src/components/DiscoveryStreamComponents/FeatureHighlight/SponsoredContentHighlight";
import React from "react";
import { mount } from "enzyme";

describe("<FeatureHighlight>", () => {
  let wrapper;
  let fakeWindow;

  beforeEach(() => {
    wrapper = mount(<FeatureHighlight />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".feature-highlight").exists());
  });

  it("should render a title", () => {
    wrapper.setProps({ message: "foo" });
    assert.ok(wrapper.find(".feature-highlight-modal p").exists());
    assert.equal(wrapper.find(".feature-highlight-modal p").text(), "foo");
  });

  it("should open a modal", () => {
    assert.ok(wrapper.find(".feature-highlight-modal.closed").exists());
    wrapper.find(".toggle-button").simulate("click");
    assert.ok(wrapper.find(".feature-highlight-modal.opened").exists());
    wrapper.find(".icon-dismiss").simulate("click");
    assert.ok(wrapper.find(".feature-highlight-modal.closed").exists());
  });

  it("should close a modal if clicking outside", () => {
    fakeWindow = {
      document: {
        addEventListener: (event, handler) => {
          fakeWindow.document.handleOutsideClick = handler;
        },
        removeEventListener: () => {},
      },
    };
    wrapper.setProps({ windowObj: fakeWindow });

    wrapper.find(".toggle-button").simulate("click");
    fakeWindow.document.handleOutsideClick({ target: null });
  });

  it("should call outsideClickCallback on Escape key press", () => {
    const outsideClickCallback = sinon.spy();

    fakeWindow = {
      document: {
        addEventListener: (event, handler) => {
          if (event === "keydown") {
            fakeWindow.document.keydownHandler = handler;
          }
        },
        removeEventListener: () => {},
      },
    };

    wrapper = mount(
      <FeatureHighlight
        windowObj={fakeWindow}
        outsideClickCallback={outsideClickCallback}
      />
    );

    // Open the modal so we can test closing it with Escape
    wrapper.find(".toggle-button").simulate("click");
    assert(wrapper.find(".feature-highlight-modal.opened").exists());

    // Simulate Escape key press
    fakeWindow.document.keydownHandler({ key: "Escape" });

    assert.calledOnce(outsideClickCallback);
    assert(wrapper.find(".feature-highlight-modal.closed").exists);
  });
});

describe("<SponsoredContentHighlight>", () => {
  let wrapper;

  beforeEach(() => {
    wrapper = mount(<SponsoredContentHighlight />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".sponsored-content-highlight").exists());
  });
});

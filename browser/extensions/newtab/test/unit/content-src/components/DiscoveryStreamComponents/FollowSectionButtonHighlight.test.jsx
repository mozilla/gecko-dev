import { FollowSectionButtonHighlight } from "content-src/components/DiscoveryStreamComponents/FeatureHighlight/FollowSectionButtonHighlight";
import { mount } from "enzyme";
import React from "react";
import { actionCreators as ac } from "common/Actions.mjs";

describe("Discovery Stream <FollowSectionButtonHighlight>", () => {
  let wrapper;
  let sandbox;
  let dispatch;
  let handleDismiss;
  let handleBlock;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    handleDismiss = sandbox.stub();
    handleBlock = sandbox.stub();

    wrapper = mount(
      <FollowSectionButtonHighlight
        dispatch={dispatch}
        handleDismiss={handleDismiss}
        handleBlock={handleBlock}
        isIntersecting={false}
        arrowPosition="arrow-inline-start"
        verticalPosition="inset-block-center"
        position="inset-inline-end"
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render highlight container", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".follow-section-button-highlight").exists());
  });

  it("should dispatch impression event when isIntersecting is true", () => {
    wrapper.setProps({ isIntersecting: true });
    wrapper.setProps({ __trigger__: Date.now() }); // force re-render to trigger useEffect

    assert(
      dispatch.calledWithMatch(
        ac.DiscoveryStreamUserEvent({
          event: "FEATURE_HIGHLIGHT_IMPRESSION",
          source: "FEATURE_HIGHLIGHT",
          value: "FEATURE_FOLLOW_SECTION_BUTTON",
        })
      )
    );
  });

  it("should dispatch dismiss event and call handleDismiss and handleBlock", () => {
    const dismissCallback = wrapper
      .find("FeatureHighlight")
      .prop("dismissCallback");

    dismissCallback();

    assert(
      dispatch.calledWithMatch(
        ac.DiscoveryStreamUserEvent({
          event: "FEATURE_HIGHLIGHT_DISMISS",
          source: "FEATURE_HIGHLIGHT",
          value: "FEATURE_FOLLOW_SECTION_BUTTON",
        })
      )
    );

    assert(handleDismiss.calledOnce);
    assert(handleBlock.calledOnce);
  });
});

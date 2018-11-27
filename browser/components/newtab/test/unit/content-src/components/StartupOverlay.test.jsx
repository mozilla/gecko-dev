import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {mountWithIntl} from "test/unit/utils";
import React from "react";
import {_StartupOverlay as StartupOverlay} from "content-src/asrouter/templates/StartupOverlay/StartupOverlay";

describe("<StartupOverlay>", () => {
  let wrapper;
  let dispatch;
  let onReady;
  let onBlock;
  beforeEach(() => {
    dispatch = sinon.stub();
    onReady = sinon.stub();
    onBlock = sinon.stub();

    wrapper = mountWithIntl(<StartupOverlay onBlock={onBlock} onReady={onReady} dispatch={dispatch} />);
  });

  it("should not render if state.show is false", () => {
    wrapper.setState({overlayRemoved: true});
    assert.isTrue(wrapper.isEmptyRender());
  });

  it("should emit UserEvent SKIPPED_SIGNIN when you click the skip button", () => {
    let skipButton = wrapper.find(".skip-button");
    assert.ok(skipButton.exists());
    skipButton.simulate("click");

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(dispatch, ac.UserEvent({event: at.SKIPPED_SIGNIN, value: {has_flow_params: false}}));
  });

  it("should emit UserEvent SUBMIT_EMAIL when you submit the form", () => {
    let form = wrapper.find("form");
    assert.ok(form.exists());
    form.simulate("submit");

    assert.calledOnce(dispatch);
    assert.isUserEventAction(dispatch.firstCall.args[0]);
    assert.calledWith(dispatch, ac.UserEvent({event: at.SUBMIT_EMAIL, value: {has_flow_params: false}}));
  });
});

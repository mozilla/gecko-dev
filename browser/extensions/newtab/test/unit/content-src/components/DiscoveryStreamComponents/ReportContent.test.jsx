import { mount } from "enzyme";
import { INITIAL_STATE, reducers } from "common/Reducers.sys.mjs";
import { ReportContent } from "content-src/components/DiscoveryStreamComponents/ReportContent/ReportContent";
import { combineReducers, createStore } from "redux";
import { Provider } from "react-redux";
import { actionCreators as ac } from "common/Actions.mjs";
import React from "react";

const DEFAULT_PROPS = {
  dispatch() {},
  prefs: {
    ...INITIAL_STATE.Prefs,
    values: {
      ...INITIAL_STATE.Prefs.values,
      "discoverystream.sections.enabled": true,
      "unifiedAds.spocs.enabled": true,
    },
  },
};

const BASE_REPORT = {
  visible: true,
  url: "https://example.com",
  position: 1,
  reporting_url: "https://example.com/report",
};

function testState({ card_type, visible }) {
  return {
    Prefs: DEFAULT_PROPS.prefs,
    DiscoveryStream: {
      ...INITIAL_STATE.DiscoveryStream,
      report: {
        ...BASE_REPORT,
        card_type,
        visible,
      },
    },
  };
}

// Wrap this around any component that uses useSelector,
// or any mount that uses a child that uses redux.
function WrapWithProvider({ children, state, dispatch }) {
  let store = createStore(combineReducers(reducers), state);
  if (dispatch) {
    store.dispatch = dispatch;
  }
  return <Provider store={store}>{children}</Provider>;
}

// patch dialog element's .showModal()/close() functions to prevent errors in tests
before(() => {
  if (typeof HTMLDialogElement !== "undefined") {
    HTMLDialogElement.prototype.showModal = function () {
      this.open = true;
    };
    HTMLDialogElement.prototype.close = function () {
      this.open = false;
    };
  }
});

describe("Discovery Stream <ReportContent>", () => {
  let wrapper;
  let sandbox;
  let dispatch;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();

    wrapper = mount(
      <WrapWithProvider>
        <ReportContent
          dispatch={dispatch}
          {...DEFAULT_PROPS}
          spocs={{ spocs: { data: {} } }}
        />
      </WrapWithProvider>
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".report-content-form").exists());
  });

  it("should open modal if report.visible is true", () => {
    const state = testState({ visible: true });

    wrapper = mount(
      <WrapWithProvider state={state}>
        <ReportContent spocs={{ spocs: { data: {} } }} />
      </WrapWithProvider>
    );
    assert.ok(wrapper.find("dialog").getDOMNode().open);
  });

  it("should close modal if report.visible is false", () => {
    const state = testState({ visible: false });

    wrapper = mount(
      <WrapWithProvider state={state}>
        <ReportContent spocs={{ spocs: { data: {} } }} />
      </WrapWithProvider>
    );

    assert.equal(wrapper.find("dialog").getDOMNode().open, false);
  });

  it("should render ad reporting options if card_type is spoc", () => {
    const state = testState({ card_type: "spoc" });

    // in the ReportContent.jsx file, spocs.spocs.data is used to grab spoc data
    wrapper = mount(
      <WrapWithProvider state={state}>
        <ReportContent spocs={{ spocs: { data: {} } }} />
      </WrapWithProvider>
    );

    assert.ok(wrapper.find(".report-ads-options").exists());

    // test to make sure content options aren't displayed when report ads is open
    assert.equal(wrapper.find(".report-content-options").length, 0);
  });

  it("should render content reporting options if card_type is organic", () => {
    const state = testState({ card_type: "organic" });

    // in the ReportContent.jsx file, spocs.spocs.data is used to grab spoc data
    wrapper = mount(
      <WrapWithProvider state={state}>
        <ReportContent spocs={{}} />
      </WrapWithProvider>
    );

    assert.ok(wrapper.find(".report-content-options").exists());

    // test to make sure ad options aren't displayed when report content is open
    assert.equal(wrapper.find(".report-ads-options").length, 0);
  });

  it("should dispatch REPORT_CLOSE when cancel button is clicked", () => {
    const state = testState({ visible: true });

    wrapper = mount(
      <WrapWithProvider state={state} dispatch={dispatch}>
        <ReportContent spocs={{}} />
      </WrapWithProvider>
    );

    // Cancel button implementation
    const cancelButton = wrapper.find("moz-button.cancel-report-btn");
    assert.ok(cancelButton.exists());
    cancelButton.simulate("click");

    assert.calledOnce(dispatch);

    const call = dispatch.getCall(0);
    assert.deepEqual(
      call.args[0],
      ac.AlsoToMain({
        type: "REPORT_CLOSE",
      })
    );
  });

  it("should dispatch REPORT_CONTENT_SUBMIT, BLOCK_URL, and SHOW_TOAST_MESSAGE when submit button is clicked", () => {
    const state = testState({ visible: true, card_type: "organic" });

    wrapper = mount(
      <WrapWithProvider state={state} dispatch={dispatch}>
        <ReportContent spocs={{}} />
      </WrapWithProvider>
    );

    // Submit button implementation
    const submitButton = wrapper.find("moz-button.submit-report-btn");
    assert.ok(submitButton.exists());
    submitButton.simulate("click");

    // Assert both action types were dispatched
    assert.calledThrice(dispatch);

    const firstCall = dispatch.getCall(0);
    const secondCall = dispatch.getCall(1);
    const thirdCall = dispatch.getCall(2);

    // Using .match instead of .deepEqual because submitting a report passes a lot of data during dispatch. And using .match makes it so we don't have to write out all the data
    assert.match(
      firstCall.args[0],
      ac.AlsoToMain({
        type: "REPORT_CONTENT_SUBMIT",
      })
    );

    assert.match(
      secondCall.args[0],
      ac.AlsoToMain({
        type: "BLOCK_URL",
      })
    );

    assert.match(
      thirdCall.args[0],
      ac.OnlyToOneContent(
        {
          type: "SHOW_TOAST_MESSAGE",
        },
        "ActivityStream:Content"
      )
    );
  });
});

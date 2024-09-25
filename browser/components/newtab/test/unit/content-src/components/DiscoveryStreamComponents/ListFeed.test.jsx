import { mount } from "enzyme";
import { INITIAL_STATE, reducers } from "common/Reducers.sys.mjs";
import { ListFeed } from "content-src/components/DiscoveryStreamComponents/ListFeed/ListFeed";
import { combineReducers, createStore } from "redux";
import { Provider } from "react-redux";
import React from "react";
import { DSCard } from "../../../../../content-src/components/DiscoveryStreamComponents/DSCard/DSCard";

const DEFAULT_PROPS = {
  type: "foo",
  firstVisibleTimestamp: new Date("March 21, 2024 10:11:12").getTime(),
  recs: [{}, {}, {}],
};

// Wrap this around any component that uses useSelector,
// or any mount that uses a child that uses redux.
function WrapWithProvider({ children, state = INITIAL_STATE }) {
  let store = createStore(combineReducers(reducers), state);
  return <Provider store={store}>{children}</Provider>;
}

describe("Discovery Stream <ListFeed>", () => {
  let wrapper;
  let sandbox;
  let dispatch;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    wrapper = mount(
      <WrapWithProvider>
        <ListFeed dispatch={dispatch} {...DEFAULT_PROPS} />
      </WrapWithProvider>
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".list-feed").exists());
  });

  it("should not render if rec prop is an empty array", () => {
    wrapper = mount(
      <WrapWithProvider>
        <ListFeed dispatch={dispatch} {...DEFAULT_PROPS} recs={[]} />
      </WrapWithProvider>
    );
    assert.ok(!wrapper.find(".list-feed").exists());
  });

  it("should render a maximum of 5 cards", () => {
    wrapper = mount(
      <WrapWithProvider>
        <ListFeed
          dispatch={dispatch}
          {...DEFAULT_PROPS}
          recs={[{}, {}, {}, {}, {}, {}, {}]}
        />
      </WrapWithProvider>
    );

    assert.lengthOf(wrapper.find(DSCard), 5);
  });

  it("should render placeholder cards if `rec` is undefined or `rec.placeholder` is true", () => {
    wrapper = mount(
      <WrapWithProvider>
        <ListFeed
          dispatch={dispatch}
          type={"foo"}
          firstVisibleTimestamp={new Date("March 21, 2024 10:11:12").getTime()}
          recs={[
            { placeholder: true },
            { placeholder: true },
            { placeholder: true },
            { placeholder: true },
            { placeholder: true },
            { placeholder: true },
          ]}
        />
      </WrapWithProvider>
    );

    assert.ok(wrapper.find(".list-card-placeholder").exists());
    assert.lengthOf(wrapper.find(".list-card-placeholder"), 5);
  });
});

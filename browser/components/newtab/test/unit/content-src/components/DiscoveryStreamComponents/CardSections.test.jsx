import React from "react";
import { mount } from "enzyme";
import { Provider } from "react-redux";
import { INITIAL_STATE, reducers } from "common/Reducers.sys.mjs";
import { CardSections } from "content-src/components/DiscoveryStreamComponents/CardSections/CardSections";
import { combineReducers, createStore } from "redux";
import { DSCard } from "../../../../../content-src/components/DiscoveryStreamComponents/DSCard/DSCard";

const DEFAULT_PROPS = {
  type: "CardGrid",
  firstVisibleTimeStamp: null,
  is_collection: true,
  spocMessageVariant: "",
  ctaButtonSponsors: [""],
  data: {
    sections: [
      {
        data: [
          {
            title: "Card 1",
            image_src: "image1.jpg",
            url: "http://example.com",
          },
          {},
          {},
          {},
        ],
        receivedRank: 0,
        sectionKey: "section_key",
        title: "title",
        layout: {
          title: "layout_name",
          responsiveLayouts: [
            {
              columnCount: 1,
              tiles: [
                {
                  size: "large",
                  position: 0,
                  hasAd: false,
                  hasExcerpt: true,
                },
                {
                  size: "small",
                  position: 2,
                  hasAd: false,
                  hasExcerpt: false,
                },
                {
                  size: "medium",
                  position: 1,
                  hasAd: true,
                  hasExcerpt: true,
                },
                {
                  size: "small",
                  position: 3,
                  hasAd: false,
                  hasExcerpt: false,
                },
              ],
            },
          ],
        },
      },
    ],
  },
  feed: {
    embed_reference: null,
    url: "https://merino.services.mozilla.com/api/v1/curated-recommendations",
  },
};

// Wrap this around any component that uses useSelector,
// or any mount that uses a child that uses redux.
function WrapWithProvider({ children, state = INITIAL_STATE }) {
  let store = createStore(combineReducers(reducers), state);
  return <Provider store={store}>{children}</Provider>;
}

describe("<CardSections />", () => {
  let wrapper;
  let sandbox;
  let dispatch;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    dispatch = sandbox.stub();
    wrapper = mount(
      <WrapWithProvider>
        <CardSections dispatch={dispatch} {...DEFAULT_PROPS} />
      </WrapWithProvider>
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render null if no data is provided", () => {
    wrapper = mount(
      <WrapWithProvider>
        <CardSections dispatch={dispatch} {...DEFAULT_PROPS} data={null} />
      </WrapWithProvider>
    );
    assert(!wrapper.find(".ds-section-wrapper").exists());
  });

  it("should render DSEmptyState if sections are falsey", () => {
    wrapper = mount(
      <WrapWithProvider>
        <CardSections
          data={{ ...DEFAULT_PROPS.data, sections: [] }}
          {...DEFAULT_PROPS}
        />
      </WrapWithProvider>
    );
    assert(!wrapper.find(".ds-card-grid empty").exists());
  });

  it("should render sections and DSCard components for valid data", () => {
    const { sections } = DEFAULT_PROPS.data;
    const sectionLength = sections.length;
    assert.lengthOf(wrapper.find("section"), sectionLength);
    assert.lengthOf(wrapper.find(DSCard), 4);
    assert.equal(wrapper.find(".section-title").text(), "title");
  });

  it("should pass correct props to DSCard", () => {
    const cardProps = wrapper.find(DSCard).at(0).props();
    assert.equal(cardProps.title, "Card 1");
    assert.equal(cardProps.image_src, "image1.jpg");
    assert.equal(cardProps.url, "http://example.com");
  });

  it("should apply correct classNames and position from layout data", () => {
    const props = wrapper.find(DSCard).at(0).props();
    const thirdProps = wrapper.find(DSCard).at(2).props();
    assert.equal(props.sectionsClassNames, "col-1-large col-1-position-0");
    assert.equal(thirdProps.sectionsClassNames, "col-1-small col-1-position-1");
  });
});

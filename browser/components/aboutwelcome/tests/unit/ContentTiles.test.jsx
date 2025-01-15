import React from "react";
import { shallow, mount } from "enzyme";
import { ContentTiles } from "content-src/components/ContentTiles";
import { ActionChecklist } from "content-src/components/ActionChecklist";
import { MobileDownloads } from "content-src/components/MobileDownloads";
import { AboutWelcomeUtils } from "content-src/lib/aboutwelcome-utils.mjs";

describe("ContentTiles component", () => {
  let sandbox;
  let wrapper;
  let handleAction;
  const CHECKLIST_TILE = {
    type: "action_checklist",
    header: {
      title: "Checklist Header",
      subtitle: "Checklist Subtitle",
      style: {
        border: "1px solid #ccc",
      },
    },
    data: [
      {
        id: "action-checklist-test",
        targeting: "false",
        label: {
          raw: "Test label",
        },
        action: {
          data: {
            pref: {
              name: "messaging-system-action.test1",
              value: "false",
            },
          },
          type: "SET_PREF",
        },
      },
    ],
  };
  const MOBILE_TILE = {
    type: "mobile_downloads",
    header: {
      title: "Mobile Header",
      style: {
        backgroundColor: "#e0e0e0",
        border: "1px solid #999",
      },
    },
    data: {
      email: {
        link_text: "Email yourself a link",
      },
    },
  };
  const TEST_CONTENT = {
    tiles: [CHECKLIST_TILE, MOBILE_TILE],
  };

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    handleAction = sandbox.stub();
    wrapper = shallow(
      <ContentTiles content={TEST_CONTENT} handleAction={handleAction} />
    );
  });

  afterEach(() => {
    sandbox.restore();
  });

  it("should render the component when tiles are provided", () => {
    assert.ok(wrapper.exists());
  });

  it("should not render the component when no tiles are provided", () => {
    wrapper.setProps({ content: {} });
    assert.ok(wrapper.isEmptyRender());
  });

  it("should render the correct number of tiles", () => {
    assert.equal(wrapper.find(".content-tile").length, 2);
  });

  it("should toggle a tile and send telemetry when its header is clicked", () => {
    let telemetrySpy = sandbox.spy(AboutWelcomeUtils, "sendActionTelemetry");
    const [firstTile] = TEST_CONTENT.tiles;
    const tileId = `${firstTile.type}${firstTile.id ? "_" : ""}${firstTile.id ?? ""}_header`;
    const firstTileButton = wrapper.find(".tile-header").at(0);
    firstTileButton.simulate("click");
    assert.equal(
      wrapper.find(".tile-content").at(0).prop("id"),
      "tile-content-0"
    );
    assert.equal(wrapper.find(".tile-content").at(0).exists(), true);
    assert.calledOnce(telemetrySpy);
    assert.equal(telemetrySpy.firstCall.args[1], tileId);
  });

  it("should only expand on tiles at a time", () => {
    const firstTileButton = wrapper.find(".tile-header").at(0);
    firstTileButton.simulate("click");
    const secondTileButton = wrapper.find(".tile-header").at(1);
    secondTileButton.simulate("click");

    assert.equal(wrapper.find(".tile-content").length, 1);
    assert.equal(
      wrapper.find(".tile-content").at(0).prop("id"),
      "tile-content-1"
    );
  });

  it("should apply configured styles to the header buttons", () => {
    const mountedWrapper = mount(
      <ContentTiles content={TEST_CONTENT} handleAction={() => {}} />
    );

    const firstTileHeader = mountedWrapper
      .find(".tile-header")
      .at(0)
      .getDOMNode();
    const secondTileHeader = mountedWrapper
      .find(".tile-header")
      .at(1)
      .getDOMNode();

    assert.equal(
      firstTileHeader.style.cssText,
      "border: 1px solid rgb(204, 204, 204);",
      "First tile header styles should match configured values"
    );
    assert.equal(
      secondTileHeader.style.cssText,
      "background-color: rgb(224, 224, 224); border: 1px solid rgb(153, 153, 153);",
      "Second tile header styles should match configured values"
    );

    mountedWrapper.unmount();
  });

  it("should render ActionChecklist for 'action_checklist' tile type", () => {
    const firstTileButton = wrapper.find(".tile-header").at(0);
    assert.ok(firstTileButton.exists(), "Tile header button should exist");
    firstTileButton.simulate("click");

    const actionChecklist = wrapper.find(ActionChecklist);
    assert.ok(actionChecklist.exists());
    assert.deepEqual(actionChecklist.prop("content").tiles[0].data, [
      {
        id: "action-checklist-test",
        targeting: "false",
        label: {
          raw: "Test label",
        },
        action: {
          data: {
            pref: {
              name: "messaging-system-action.test1",
              value: "false",
            },
          },
          type: "SET_PREF",
        },
      },
    ]);
  });

  it("should render MobileDownloads for 'mobile_downloads' tile type", () => {
    const secondTileButton = wrapper.find(".tile-header").at(1);
    assert.ok(secondTileButton.exists(), "Tile header button should exist");
    secondTileButton.simulate("click");

    const mobileDownloads = wrapper.find(MobileDownloads);
    assert.ok(mobileDownloads.exists());
    assert.deepEqual(mobileDownloads.prop("data"), {
      email: {
        link_text: "Email yourself a link",
      },
    });
    assert.equal(mobileDownloads.prop("handleAction"), handleAction);
  });

  it("should handle a single tile object", () => {
    wrapper.setProps({
      content: {
        tiles: {
          type: "action_checklist",
          data: [
            {
              id: "action-checklist-single",
              targeting: "false",
              label: {
                raw: "Single tile label",
              },
              action: {
                data: {
                  pref: {
                    name: "messaging-system-action.single",
                    value: "false",
                  },
                },
                type: "SET_PREF",
              },
            },
          ],
        },
      },
    });

    const actionChecklist = wrapper.find(ActionChecklist);
    assert.ok(actionChecklist.exists());
    assert.deepEqual(actionChecklist.prop("content").tiles.data, [
      {
        id: "action-checklist-single",
        targeting: "false",
        label: {
          raw: "Single tile label",
        },
        action: {
          data: {
            pref: {
              name: "messaging-system-action.single",
              value: "false",
            },
          },
          type: "SET_PREF",
        },
      },
    ]);
  });
});

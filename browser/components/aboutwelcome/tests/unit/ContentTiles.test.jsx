import React from "react";
import { shallow, mount } from "enzyme";
import { ContentTiles } from "content-src/components/ContentTiles";
import { ActionChecklist } from "content-src/components/ActionChecklist";
import { MobileDownloads } from "content-src/components/MobileDownloads";
import { AboutWelcomeUtils } from "content-src/lib/aboutwelcome-utils.mjs";
import { GlobalOverrider } from "asrouter/tests/unit/utils";

describe("ContentTiles component", () => {
  let sandbox;
  let wrapper;
  let handleAction;
  let setActiveMultiSelect;
  let setActiveSingleSelect;
  let globals;

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

  const TITLE_TILE = {
    type: "mobile_downloads",
    title: "Tile Title",
    subtitle: "Tile Subtitle",
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
    setActiveMultiSelect = sandbox.stub();
    setActiveSingleSelect = sandbox.stub();
    globals = new GlobalOverrider();
    globals.set({
      AWSendToDeviceEmailsSupported: () => Promise.resolve(),
    });
    wrapper = shallow(
      <ContentTiles
        content={TEST_CONTENT}
        handleAction={handleAction}
        activeMultiSelect={null}
        setActiveMultiSelect={setActiveMultiSelect}
      />
    );
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
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
    const tileId = `${firstTile.type}${firstTile.id ? "_" : ""}${
      firstTile.id ?? ""
    }_header`;
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

  it("should only expand one tile at a time", () => {
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

  it("should toggle all tiles and send telemetry when the tiles header is clicked", () => {
    const TEST_CONTENT_HEADER = {
      tiles: [CHECKLIST_TILE, MOBILE_TILE],
      tiles_header: {
        title: "Toggle Tiles Header",
      },
    };

    wrapper = mount(
      <ContentTiles
        content={TEST_CONTENT_HEADER}
        handleAction={handleAction}
        activeMultiSelect={null}
        setActiveMultiSelect={setActiveMultiSelect}
      />
    );

    let telemetrySpy = sandbox.spy(AboutWelcomeUtils, "sendActionTelemetry");
    const tilesHeaderButton = wrapper.find(".content-tiles-header");

    assert.ok(tilesHeaderButton.exists(), "Tiles header button should exist");
    tilesHeaderButton.simulate("click");

    assert.equal(
      wrapper.find("#content-tiles-container").exists(),
      true,
      "Content tiles container should be visible after toggle"
    );
    assert.calledOnce(telemetrySpy);
    assert.equal(
      telemetrySpy.firstCall.args[1],
      "content_tiles_header",
      "Telemetry should be sent for tiles header toggle"
    );

    tilesHeaderButton.simulate("click");
    assert.equal(
      wrapper.find("#content-tiles-container").exists(),
      false,
      "Content tiles container should not be visible after second toggle"
    );
  });

  it("should apply configured styles to the header buttons", () => {
    const mountedWrapper = mount(
      <ContentTiles
        content={TEST_CONTENT}
        handleAction={() => {}}
        activeMultiSelect={null}
        setActiveMultiSelect={setActiveMultiSelect}
      />
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

  it("should prefill activeMultiSelect for a MultiSelect tile based on default values", () => {
    const MULTISELECT_TILE = {
      type: "multiselect",
      header: {
        title: "Multiselect Header",
        style: {
          border: "1px solid #ddd",
        },
      },
      data: [
        { id: "option1", defaultValue: true },
        { id: "option2", defaultValue: false },
        { id: "option3", defaultValue: true },
      ],
    };

    const contentWithMultiselect = { tiles: MULTISELECT_TILE };

    wrapper = mount(
      <ContentTiles
        content={contentWithMultiselect}
        activeMultiSelect={null}
        setActiveMultiSelect={setActiveMultiSelect}
        handleAction={handleAction}
      />
    );
    wrapper.update();

    sinon.assert.calledOnce(setActiveMultiSelect);
    sinon.assert.calledWithExactly(
      setActiveMultiSelect,
      ["option1", "option3"],
      "tile-0"
    );
  });

  it("should not prefill activeMultiSelect if it is already set", () => {
    const MULTISELECT_TILE = {
      type: "multiselect",
      header: {
        title: "Multiselect Header",
        style: {
          border: "1px solid #ddd",
        },
      },
      data: [
        { id: "option1", defaultValue: true },
        { id: "option2", defaultValue: false },
        { id: "option3", defaultValue: true },
      ],
    };

    const contentWithMultiselect = { tiles: [MULTISELECT_TILE] };

    wrapper = mount(
      <ContentTiles
        content={contentWithMultiselect}
        activeMultiSelect={["option2"]}
        setActiveMultiSelect={setActiveMultiSelect}
        handleAction={handleAction}
      />
    );
    wrapper.update();

    sinon.assert.notCalled(setActiveMultiSelect);
  });

  it("should render title and subtitle if present", () => {
    sandbox.stub(window, "AWSendToDeviceEmailsSupported").resolves(true);

    let TEST_TILE_CONTENT = {
      tiles: [TITLE_TILE],
    };

    const mountedWrapper = mount(
      <ContentTiles
        content={TEST_TILE_CONTENT}
        handleAction={() => {}}
        activeMultiSelect={null}
        setActiveMultiSelect={setActiveMultiSelect}
      />
    );

    const tileTitle = mountedWrapper.find(".tile-title");
    const tileSubtitle = mountedWrapper.find(".tile-subtitle");

    assert.ok(tileTitle.exists(), "Title should render");
    assert.ok(tileSubtitle.exists(), "Subtitle should render");

    assert.equal(
      tileTitle.text(),
      "Tile Title",
      "Tile title should have correct text"
    );
    assert.equal(
      tileSubtitle.text(),
      "Tile Subtitle",
      "Tile subtitle should have correct text"
    );

    mountedWrapper.unmount();
  });

  it("should render multiple title and subtitles if multiple tiles contain them", () => {
    sandbox.stub(window, "AWSendToDeviceEmailsSupported").resolves(true);
    const SECOND_TITLE_TILE = {
      type: "mobile_downloads",
      title: "Tile Title 2",
      subtitle: "Tile Subtitle 2",
      data: {
        email: {
          link_text: "Email yourself a link",
        },
      },
    };

    let MULTIPLE_TILES_CONTENT = {
      tiles: [TITLE_TILE, SECOND_TITLE_TILE],
    };

    const mountedWrapper = mount(
      <ContentTiles
        content={MULTIPLE_TILES_CONTENT}
        handleAction={() => {}}
        activeMultiSelect={null}
        setActiveMultiSelect={setActiveMultiSelect}
        setActiveSingleSelect={setActiveSingleSelect}
      />
    );

    const tileTitles = mountedWrapper.find(".tile-title");
    const tileSubtitles = mountedWrapper.find(".tile-subtitle");

    assert.equal(tileTitles.length, 2, "Should render two tile titles");
    assert.equal(tileSubtitles.length, 2, "Should render two tile subtitles");

    assert.equal(
      tileTitles.at(0).text(),
      "Tile Title",
      "First tile title should have correct text"
    );
    assert.equal(
      tileSubtitles.at(0).text(),
      "Tile Subtitle",
      "First tile subtitle should have correct text"
    );

    assert.equal(
      tileTitles.at(1).text(),
      "Tile Title 2",
      "Second tile title should have correct text"
    );
    assert.equal(
      tileSubtitles.at(1).text(),
      "Tile Subtitle 2",
      "Second tile subtitle should have correct text"
    );

    mountedWrapper.unmount();
  });

  it("should pre-populate multiple MultiSelect components in the same ContentTiles independently of one another", () => {
    const FIRST_MULTISELECT_TILE = {
      type: "multiselect",
      header: {
        title: "First Multiselect",
      },
      data: [
        { id: "checklist1-option1", defaultValue: true },
        { id: "checklist1-option2", defaultValue: false },
      ],
    };

    const SECOND_MULTISELECT_TILE = {
      type: "multiselect",
      header: {
        title: "Second Multiselect",
      },
      data: [
        { id: "checklist2-option1", defaultValue: false },
        { id: "checklist2-option2", defaultValue: true },
      ],
    };

    const contentWithMultipleMultiselects = {
      tiles: [FIRST_MULTISELECT_TILE, SECOND_MULTISELECT_TILE],
    };

    wrapper = mount(
      <ContentTiles
        content={contentWithMultipleMultiselects}
        activeMultiSelect={null}
        setActiveMultiSelect={setActiveMultiSelect}
        handleAction={handleAction}
      />
    );
    wrapper.update();

    sinon.assert.calledWithExactly(
      setActiveMultiSelect.getCall(0),
      ["checklist1-option1"],
      "tile-0"
    );
    sinon.assert.calledWithExactly(
      setActiveMultiSelect.getCall(1),
      ["checklist2-option2"],
      "tile-1"
    );

    wrapper.unmount();
  });

  it("should handle interaction between multiselect instances independently of one another", () => {
    const FIRST_MULTISELECT_TILE = {
      type: "multiselect",
      header: {
        title: "First Multiselect",
      },
      data: [
        { id: "checklist1-option1", defaultValue: true, label: "Option 1-1" },
        { id: "checklist1-option2", defaultValue: false, label: "Option 1-2" },
      ],
    };

    const SECOND_MULTISELECT_TILE = {
      type: "multiselect",
      header: {
        title: "Second Multiselect",
      },
      data: [
        { id: "checklist2-option1", defaultValue: false, label: "Option 2-1" },
        { id: "checklist2-option2", defaultValue: true, label: "Option 2-2" },
      ],
    };

    const contentWithMultipleMultiselects = {
      tiles: [FIRST_MULTISELECT_TILE, SECOND_MULTISELECT_TILE],
    };

    const initialActiveMultiSelect = {
      "tile-0": ["checklist1-option1"],
      "tile-1": ["checklist2-option2"],
    };

    const setScreenMultiSelects = sandbox.stub();

    wrapper = mount(
      <ContentTiles
        content={contentWithMultipleMultiselects}
        activeMultiSelect={initialActiveMultiSelect}
        setActiveMultiSelect={setActiveMultiSelect}
        setScreenMultiSelects={setScreenMultiSelects}
        handleAction={handleAction}
      />
    );

    // First multiselect
    const firstTileButton = wrapper.find(".tile-header").at(0);
    assert.ok(firstTileButton.exists(), "First tile header should exist");
    firstTileButton.simulate("click");

    const firstChecklistInputs = wrapper.find(".multi-select-container input");
    assert.equal(
      firstChecklistInputs.length,
      2,
      "First checklist should have 2 inputs"
    );

    assert.equal(
      firstChecklistInputs.at(0).prop("checked"),
      true,
      "First option should be checked"
    );
    assert.equal(
      firstChecklistInputs.at(1).prop("checked"),
      false,
      "Second option should be unchecked"
    );

    const secondInput = firstChecklistInputs.at(1);
    secondInput.getDOMNode().checked = true;
    secondInput.simulate("change");

    sinon.assert.calledOnce(setActiveMultiSelect);
    sinon.assert.calledWithExactly(
      setActiveMultiSelect,
      ["checklist1-option1", "checklist1-option2"],
      "tile-0"
    );
    setActiveMultiSelect.reset();

    // Second multiSelect
    const secondTileButton = wrapper.find(".tile-header").at(1);
    secondTileButton.simulate("click");

    const secondChecklistInputs = wrapper.find(".multi-select-container input");
    assert.equal(
      secondChecklistInputs.length,
      2,
      "Second checklist should have 2 inputs"
    );

    assert.equal(
      secondChecklistInputs.at(0).prop("checked"),
      false,
      "First option should be unchecked"
    );
    assert.equal(
      secondChecklistInputs.at(1).prop("checked"),
      true,
      "Second option should be checked"
    );

    // Uncheck the second checkbox in the second multiselect
    const lastInput = secondChecklistInputs.at(1);
    lastInput.getDOMNode().checked = false;
    lastInput.simulate("change");

    sinon.assert.calledOnce(setActiveMultiSelect);
    sinon.assert.calledWithExactly(setActiveMultiSelect, [], "tile-1");

    // Ensure the first multiselect's state wasn't altered by changes to the second multiselect
    wrapper.setProps({
      activeMultiSelect: {
        "tile-0": ["checklist1-option1", "checklist1-option2"],
        "tile-1": [],
      },
    });

    firstTileButton.simulate("click");

    // Get the updated checkboxes from the first checklist
    const updatedFirstInputs = wrapper.find(".multi-select-container input");

    assert.equal(
      updatedFirstInputs.at(0).prop("checked"),
      true,
      "First option should still be checked"
    );
    assert.equal(
      updatedFirstInputs.at(1).prop("checked"),
      true,
      "Second option should still be checked"
    );

    wrapper.unmount();
  });
});

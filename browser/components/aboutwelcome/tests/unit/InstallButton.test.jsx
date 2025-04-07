import React from "react";
import { shallow } from "enzyme";
import { InstallButton } from "content-src/components/InstallButton";

const TEST_ADDON_INFO = [
  {
    name: "Test Add-on",
    id: "d634138d-c276-4fc8-924b-40a0ea21d284",
    url: "http://example.com",
    icons: { 32: "test.png", 64: "test.png" },
    type: "extension",
  },
];

describe("InstallButton component", () => {
  let sandbox;
  let wrapper;
  let handleAction;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    handleAction = sandbox.stub();
    wrapper = shallow(
      <InstallButton
        key={TEST_ADDON_INFO.id}
        addonId={TEST_ADDON_INFO.id}
        addonType={TEST_ADDON_INFO.type}
        addonName={TEST_ADDON_INFO.name}
        index={"primary_button"}
        handleAction={handleAction}
        installedAddons={[]}
      />
    );
  });

  it("should render InstallButton component", () => {
    assert.ok(wrapper.exists());
  });

  it("should render the button with the correct value", () => {
    assert.lengthOf(wrapper.find("button[value='primary_button']"), 1);
  });

  it("should call handleAction method when button is link is clicked", () => {
    const btnLink = wrapper.find("button.primary");
    btnLink.simulate("click");
    assert.calledOnce(handleAction);
  });
});

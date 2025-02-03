import { Logo } from "content-src/components/Logo/Logo";
import React from "react";
import { mount } from "enzyme";

describe("<Logo>", () => {
  let wrapper;
  //   let fakeWindow;

  beforeEach(() => {
    wrapper = mount(<Logo />);
  });

  it("should render", () => {
    assert.ok(wrapper.exists());
    assert.ok(wrapper.find(".logo-and-wordmark").exists());
  });

  it("should render a logo and wordmark element", () => {
    assert.ok(wrapper.find(".logo-and-wordmark .logo").exists());
    assert.ok(wrapper.find(".logo-and-wordmark .wordmark").exists());
  });
});

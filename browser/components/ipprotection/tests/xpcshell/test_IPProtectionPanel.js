/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { IPProtectionPanel } = ChromeUtils.importESModule(
  "resource:///modules/ipprotection/IPProtectionPanel.sys.mjs"
);

class FakeIPProtectionPanelElement {
  constructor() {
    this.state = {};
    this.isConnected = false;
  }

  closest() {
    return {
      state: "open",
    };
  }
}

/**
 * Tests that we can set a state and pass it to a fake element.
 */
add_task(async function test_setState() {
  let ipProtectionPanel = new IPProtectionPanel();
  let fakeElement = new FakeIPProtectionPanelElement();
  ipProtectionPanel.panel = fakeElement;

  ipProtectionPanel.setState({
    foo: "bar",
  });

  Assert.deepEqual(
    ipProtectionPanel.state,
    { foo: "bar" },
    "The state should be set on the IPProtectionPanel instance"
  );

  Assert.deepEqual(
    fakeElement.state,
    {},
    "The state should not be set on the fake element, as it is not connected"
  );

  fakeElement.isConnected = true;

  ipProtectionPanel.setState({
    isFoo: true,
  });

  Assert.deepEqual(
    ipProtectionPanel.state,
    { foo: "bar", isFoo: true },
    "The state should be set on the IPProtectionPanel instance"
  );

  Assert.deepEqual(
    fakeElement.state,
    { foo: "bar", isFoo: true },
    "The state should be set on the fake element"
  );
});

/**
 * Tests that the whole state will be updated when calling updateState directly.
 */
add_task(async function test_updateState() {
  let ipProtectionPanel = new IPProtectionPanel();
  let fakeElement = new FakeIPProtectionPanelElement();
  ipProtectionPanel.panel = fakeElement;

  ipProtectionPanel.setState({
    foo: "bar",
  });

  Assert.deepEqual(
    fakeElement.state,
    {},
    "The state should not be set on the fake element, as it is not connected"
  );

  fakeElement.isConnected = true;
  ipProtectionPanel.updateState();

  Assert.deepEqual(
    fakeElement.state,
    { foo: "bar" },
    "The state should be set on the fake element"
  );
});

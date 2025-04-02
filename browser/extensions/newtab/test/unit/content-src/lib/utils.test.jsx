import React from "react";
import { mount } from "enzyme";
import {
  useIntersectionObserver,
  getActiveCardSize,
} from "content-src/lib/utils.jsx";

// Test component to use the useIntersectionObserver
function TestComponent({ callback, threshold }) {
  const ref = useIntersectionObserver(callback, threshold);
  return <div ref={el => ref.current.push(el)}></div>;
}

describe("useIntersectionObserver", () => {
  let callback;
  let threshold;
  let sandbox;
  let observerStub;
  let wrapper;

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    callback = sandbox.spy();
    threshold = 0.5;
    observerStub = sandbox
      .stub(window, "IntersectionObserver")
      .callsFake(function (cb) {
        this.observe = sandbox.spy();
        this.unobserve = sandbox.spy();
        this.disconnect = sandbox.spy();
        this.callback = cb;
      });
    wrapper = mount(
      <TestComponent callback={callback} threshold={threshold} />
    );
  });

  afterEach(() => {
    sandbox.restore();
    wrapper.unmount();
  });

  it("should create an IntersectionObserver instance with the correct options", () => {
    assert.calledWithNew(observerStub);
    assert.calledWith(observerStub, sinon.match.any, { threshold });
  });

  it("should observe elements when mounted", () => {
    const observerInstance = observerStub.getCall(0).returnValue;
    assert.called(observerInstance.observe);
  });

  it("should call callback and unobserve element when it intersects", () => {
    wrapper = mount(
      <TestComponent callback={callback} threshold={threshold} />
    );
    const observerInstance = observerStub.getCall(0).returnValue;
    const observedElement = wrapper.find("div").getDOMNode();

    // Simulate an intersection
    observerInstance.callback([
      { isIntersecting: true, target: observedElement },
    ]);

    assert.calledOnce(callback);
    assert.calledWith(callback, observedElement);
    assert.calledOnce(observerInstance.unobserve);
    assert.calledWith(observerInstance.unobserve, observedElement);
  });

  it("should not call callback if element is not intersecting", () => {
    wrapper = mount(
      <TestComponent callback={callback} threshold={threshold} />
    );
    const observerInstance = observerStub.getCall(0).returnValue;
    const observedElement = wrapper.find("div").getDOMNode();

    // Simulate a non-intersecting entry
    observerInstance.callback([
      { isIntersecting: false, target: observedElement },
    ]);

    assert.notCalled(callback);
    assert.notCalled(observerInstance.unobserve);
  });
});

describe("getActiveCardSize", () => {
  it("returns 'large-card' for col-4-large and screen width 1920 and sections enabled", () => {
    const result = getActiveCardSize(
      1920,
      "col-4-large col-3-medium col-2-small col-1-small",
      true
    );
    assert.equal(result, "large-card");
  });

  it("returns 'medium-card' for col-3-medium and screen width 1200 and sections enabled", () => {
    const result = getActiveCardSize(
      1200,
      "col-4-large col-3-medium col-2-small col-1-small",
      true
    );
    assert.equal(result, "medium-card");
  });

  it("returns 'small-card' for col-2-small and screen width 800 and sections enabled", () => {
    const result = getActiveCardSize(
      800,
      "col-4-large col-3-medium col-2-small col-1-medium",
      true
    );
    assert.equal(result, "small-card");
  });

  it("returns 'medium-card' for col-1-medium at 500px", () => {
    const result = getActiveCardSize(
      500,
      "col-1-medium col-1-position-0",
      true
    );
    assert.equal(result, "medium-card");
  });

  it("returns 'medium-card' for col-1-small at 500px (edge case)", () => {
    const result = getActiveCardSize(500, "col-1-small col-1-position-0", true);
    assert.equal(result, "medium-card");
  });

  it("returns null when no matching card type is found (edge case)", () => {
    const result = getActiveCardSize(
      1200,
      "col-4-position-0 col-3-position-0",
      true
    );
    assert.isNull(result);
  });

  it("returns 'medium-card' when required arguments are missing and sections are disabled", () => {
    const result = getActiveCardSize(null, null, false);
    assert.equal(result, "medium-card");
  });

  it("returns null when required arguments are missing and sections are enabled", () => {
    const result = getActiveCardSize(null, null, true);
    assert.isNull(result);
  });

  it("returns 'spoc' when flightId has value", () => {
    const result = getActiveCardSize(null, null, false, 123);
    assert.equal(result, "spoc");
  });
});

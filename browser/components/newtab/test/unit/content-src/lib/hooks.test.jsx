import React from "react";
import { mount } from "enzyme";
import { useIntersectionObserver } from "content-src/lib/hooks.jsx";

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

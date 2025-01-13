import {
  ASRouterAdminInner,
  toBinary,
} from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import { ASRouterUtils } from "content-src/asrouter-utils";
import { GlobalOverrider } from "tests/unit/utils";
import React from "react";
import { mount } from "enzyme";

describe("ASRouterAdmin", () => {
  let globalOverrider;
  let sandbox;
  let wrapper;
  let globals;
  let FAKE_PROVIDER_PREF = [
    {
      enabled: true,
      id: "local_testing",
      localProvider: "TestProvider",
      type: "local",
    },
  ];
  let FAKE_PROVIDER = [
    {
      enabled: true,
      id: "local_testing",
      localProvider: "TestProvider",
      messages: [],
      type: "local",
    },
  ];
  beforeEach(() => {
    globalOverrider = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    sandbox.stub(ASRouterUtils, "getPreviewEndpoint").returns("foo");
    globals = {
      ASRouterMessage: sandbox.stub().resolves(),
      ASRouterAddParentListener: sandbox.stub(),
      ASRouterRemoveParentListener: sandbox.stub(),
    };
    globalOverrider.set(globals);
    wrapper = mount(<ASRouterAdminInner location={{ routes: [""] }} />);
    wrapper.setState({ devtoolsEnabled: true, messages: [] });
  });
  afterEach(() => {
    sandbox.restore();
    globalOverrider.restore();
  });
  it("should render ASRouterAdmin component", () => {
    assert.ok(wrapper.exists());
  });
  it("should send ADMIN_CONNECT_STATE on mount", () => {
    assert.calledOnce(globals.ASRouterMessage);
    assert.calledWith(globals.ASRouterMessage, {
      type: "ADMIN_CONNECT_STATE",
      data: { endpoint: "foo" },
    });
  });
  describe("#getSection", () => {
    it("should render a message provider section by default", () => {
      assert.lengthOf(wrapper.find(".messages-list").at(0), 1);
    });
    it("should render a targeting section for targeting route", () => {
      wrapper = mount(
        <ASRouterAdminInner location={{ routes: ["targeting"] }} />
      );
      wrapper.setState({ devtoolsEnabled: true });
      assert.lengthOf(wrapper.find(".targeting-table").at(0), 1);
    });
    it("should render two error messages", () => {
      wrapper = mount(
        <ASRouterAdminInner location={{ routes: ["errors"] }} Sections={[]} />
      );
      wrapper.setState({ devtoolsEnabled: true });
      const firstError = {
        timestamp: Date.now() + 100,
        error: { message: "first" },
      };
      const secondError = {
        timestamp: Date.now(),
        error: { message: "second" },
      };
      wrapper.setState({
        providers: [{ id: "foo", errors: [firstError, secondError] }],
      });

      assert.equal(
        wrapper.find("tbody tr").at(0).find("td").at(0).text(),
        "foo"
      );
      assert.lengthOf(wrapper.find("tbody tr"), 2);
      assert.equal(
        wrapper.find("tbody tr").at(0).find("td").at(1).text(),
        secondError.error.message
      );
    });
  });
  describe("#render", () => {
    beforeEach(() => {
      wrapper.setState({
        providerPrefs: [],
        providers: [],
        userPrefs: {},
      });
    });
    describe("#renderProviders", () => {
      it("should render the provider", () => {
        wrapper.setState({
          providerPrefs: FAKE_PROVIDER_PREF,
          providers: FAKE_PROVIDER,
        });

        assert.lengthOf(wrapper.find(`[data-provider]`), 1);
      });
    });
    describe("#renderMessages", () => {
      beforeEach(() => {
        sandbox.stub(ASRouterUtils, "blockById").resolves();
        sandbox.stub(ASRouterUtils, "unblockById").resolves();
        sandbox.stub(ASRouterUtils, "overrideMessage").resolves({ foo: "bar" });
        sandbox.stub(ASRouterUtils, "sendMessage").resolves();
        wrapper.setState({
          filterProviders: [],
          filterGroups: [],
          filterTemplates: [],
          filtersCollapsed: false,
          messageBlockList: [],
          messageImpressions: { foo: 2 },
          groups: [{ id: "messageProvider", enabled: true }],
          providers: [
            { id: "messageProvider", enabled: true },
            { id: "nullProvider", enabled: true },
          ],
        });
      });
      it("should render a message when no filtering is applied", () => {
        wrapper.setState({
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button.primary").simulate("click");
        assert.calledOnce(ASRouterUtils.overrideMessage);
        assert.calledWith(ASRouterUtils.overrideMessage, "foo");
      });
      it("should render a blocked message", () => {
        wrapper.setState({
          messages: [
            {
              id: "foo",
              groups: ["messageProvider"],
              provider: "messageProvider",
            },
          ],
          messageBlockList: ["foo"],
        });
        assert.lengthOf(wrapper.find(".message-item.blocked"), 1);
        wrapper.find(".message-item.blocked button.primary").simulate("click");
        assert.calledOnce(ASRouterUtils.unblockById);
        assert.calledWith(ASRouterUtils.unblockById, "foo");
      });
      it("should render a message if it matches filter", () => {
        wrapper.setState({
          filterProviders: ["messageProvider"],
          filterGroups: ["messageProvider"],
          filterTemplates: ["bar"],
          messages: [
            {
              id: "foo",
              template: "bar",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
      });
      it("should override with the selected message", async () => {
        wrapper.setState({
          filterProviders: ["messageProvider"],
          filterGroups: ["messageProvider"],
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);
        wrapper.find(".message-item button.show").simulate("click");
        assert.calledOnce(ASRouterUtils.overrideMessage);
        assert.calledWith(ASRouterUtils.overrideMessage, "foo");
        await ASRouterUtils.overrideMessage();
        assert.equal(wrapper.state().foo, "bar");
      });
      it("should hide message if provider filter changes", () => {
        wrapper.setState({
          filterProviders: ["messageProvider"],
          filterGroups: ["messageProvider"],
          messages: [
            {
              id: "foo",
              provider: "messageProvider",
              groups: ["messageProvider"],
            },
          ],
        });

        assert.lengthOf(wrapper.find(".message-id"), 1);

        let ckbx1 = wrapper.find("[data-provider='messageProvider']");
        ckbx1.getDOMNode().checked = false;
        ckbx1.simulate("change");
        let ckbx2 = wrapper.find("[data-provider='nullProvider']");
        ckbx2.getDOMNode().checked = true;
        ckbx2.simulate("change");

        assert.lengthOf(wrapper.find(".message-id"), 0);
      });
    });
  });
  describe("toBinary", () => {
    // Bringing the 'fromBinary' function over from
    // messagepreview to prove it works
    function fromBinary(encoded) {
      const binary = atob(decodeURIComponent(encoded));
      const bytes = new Uint8Array(binary.length);
      for (let i = 0; i < bytes.length; i++) {
        bytes[i] = binary.charCodeAt(i);
      }
      return String.fromCharCode(...new Uint16Array(bytes.buffer));
    }

    it("correctly encodes a latin string", () => {
      const testString = "Hi I am a test string";
      const expectedResult =
        "SABpACAASQAgAGEAbQAgAGEAIAB0AGUAcwB0ACAAcwB0AHIAaQBuAGcA";

      const encodedResult = toBinary(testString);

      assert.equal(encodedResult, expectedResult);

      const decodedResult = fromBinary(encodedResult);

      assert.equal(decodedResult, testString);
    });

    it("correctly encodes a non-latin string", () => {
      const nonLatinString = "тестовое сообщение";
      const expectedResult = "QgQ1BEEEQgQ+BDIEPgQ1BCAAQQQ+BD4EMQRJBDUEPQQ4BDUE";

      const encodedResult = toBinary("тестовое сообщение");

      assert.equal(encodedResult, expectedResult);

      const decodedResult = fromBinary(encodedResult);

      assert.equal(decodedResult, nonLatinString);
    });
  });
});

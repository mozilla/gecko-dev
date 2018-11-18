import {GlobalOverrider} from "test/unit/utils";
import {OnboardingMessageProvider} from "lib/OnboardingMessageProvider.jsm";
import schema from "content-src/asrouter/templates/OnboardingMessage/OnboardingMessage.schema.json";

const DEFAULT_CONTENT = {
  "title": "A title",
  "text": "A description",
  "icon": "icon",
  "button_label": "some_button_label",
  "button_action": {
    "type": "SOME_TYPE",
    "data": {"args": "example.com"},
  },
};

const L10N_CONTENT = {
  "title": {string_id: "onboarding-private-browsing-title"},
  "text": {string_id: "onboarding-private-browsing-text"},
  "icon": "icon",
  "button_label": {string_id: "onboarding-button-label-try-now"},
  "button_action": {type: "SOME_TYPE"},
};

describe("OnboardingMessage", () => {
  let globals;
  let sandbox;
  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.sandbox.create();
    globals.set("FxAccountsConfig", {promiseEmailFirstURI: sandbox.stub().resolves("some/url")});
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });
  it("should validate DEFAULT_CONTENT", () => {
    assert.jsonSchema(DEFAULT_CONTENT, schema);
  });
  it("should validate L10N_CONTENT", () => {
    assert.jsonSchema(L10N_CONTENT, schema);
  });
  it("should validate all messages from OnboardingMessageProvider", async () => {
    const messages = await OnboardingMessageProvider.getUntranslatedMessages();
    messages.forEach(msg => assert.jsonSchema(msg.content, schema));
  });
});

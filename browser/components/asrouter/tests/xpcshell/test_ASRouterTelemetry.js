/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MESSAGE_TYPE_HASH: msg } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ActorConstants.mjs"
);

const { ASRouterTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterTelemetry.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  AboutWelcomeTelemetry:
    "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  JsonSchemaValidator:
    "resource://gre/modules/components-utils/JsonSchemaValidator.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
  TelemetryController: "resource://gre/modules/TelemetryController.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
});

const FAKE_UUID = "{foo-123-foo}";
const PREF_TELEMETRY = "browser.newtabpage.activity-stream.telemetry";

let ASRouterEventPingSchemaPromise;

function assertPingMatchesSchema(pingKind, ping, schema) {
  // Unlike the validator from JsonSchema.sys.mjs, JsonSchemaValidator
  // lets us opt-in to having "undefined" properties, which are then
  // ignored. This is fine because the ping is sent as a JSON string
  // over an XHR, and undefined properties are culled as part of the
  // JSON encoding process.
  let result = JsonSchemaValidator.validate(ping, schema, {
    allowExplicitUndefinedProperties: true,
  });

  if (!result.valid) {
    info(`${pingKind} failed to validate against the schema: ${result.error}`);
  }

  Assert.ok(result.valid, `${pingKind} is valid against the schema.`);
}

async function assertASRouterEventPingValid(ping) {
  let schema = await ASRouterEventPingSchemaPromise;
  assertPingMatchesSchema("ASRouterEventPing", ping, schema);
}

add_setup(async function setup() {
  ASRouterEventPingSchemaPromise = IOUtils.readJSON(
    do_get_file("../schemas/asrouter_event_ping.schema.json").path
  );

  do_get_profile();

  await TelemetryController.testReset();
});

add_task(async function test_applyCFRPolicy_prerelease() {
  info(
    "ASRouterTelemetry.applyCFRPolicy should use client_id and message_id " +
      "in prerelease"
  );
  let sandbox = sinon.createSandbox();
  sandbox.stub(UpdateUtils, "getUpdateChannel").returns("nightly");

  let instance = new ASRouterTelemetry();

  let data = {
    action: "cfr_user_event",
    event: "IMPRESSION",
    message_id: "cfr_message_01",
    bucket_id: "cfr_bucket_01",
  };
  let { ping, pingType } = await instance.applyCFRPolicy(data);

  Assert.equal(pingType, "cfr");
  Assert.equal(ping.impression_id, undefined);
  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(ping.bucket_id, "cfr_bucket_01");
  Assert.equal(ping.message_id, "cfr_message_01");

  sandbox.restore();
});

add_task(async function test_applyCFRPolicy_release() {
  info(
    "ASRouterTelemetry.applyCFRPolicy should use impression_id and bucket_id " +
      "in release"
  );
  let sandbox = sinon.createSandbox();
  sandbox.stub(UpdateUtils, "getUpdateChannel").returns("release");
  sandbox
    .stub(ASRouterTelemetry.prototype, "getOrCreateImpressionId")
    .returns(FAKE_UUID);

  let instance = new ASRouterTelemetry();

  let data = {
    action: "cfr_user_event",
    event: "IMPRESSION",
    message_id: "cfr_message_01",
    bucket_id: "cfr_bucket_01",
  };
  let { ping, pingType } = await instance.applyCFRPolicy(data);

  Assert.equal(pingType, "cfr");
  Assert.equal(ping.impression_id, FAKE_UUID);
  Assert.equal(ping.client_id, undefined);
  Assert.equal(ping.bucket_id, "cfr_bucket_01");
  Assert.equal(ping.message_id, "n/a");

  sandbox.restore();
});

add_task(async function test_applyCFRPolicy_experiment_release() {
  info(
    "ASRouterTelemetry.applyCFRPolicy should use impression_id and bucket_id " +
      "in release"
  );
  let sandbox = sinon.createSandbox();
  sandbox.stub(UpdateUtils, "getUpdateChannel").returns("release");
  sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
    slug: "SOME-CFR-EXP",
  });

  let instance = new ASRouterTelemetry();

  let data = {
    action: "cfr_user_event",
    event: "IMPRESSION",
    message_id: "cfr_message_01",
    bucket_id: "cfr_bucket_01",
  };
  let { ping, pingType } = await instance.applyCFRPolicy(data);

  Assert.equal(pingType, "cfr");
  Assert.equal(ping.impression_id, undefined);
  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(ping.bucket_id, "cfr_bucket_01");
  Assert.equal(ping.message_id, "cfr_message_01");

  sandbox.restore();
});

add_task(async function test_applyCFRPolicy_release_private_browsing() {
  info(
    "ASRouterTelemetry.applyCFRPolicy should use impression_id and bucket_id " +
      "in Private Browsing in release"
  );
  let sandbox = sinon.createSandbox();
  sandbox.stub(UpdateUtils, "getUpdateChannel").returns("release");
  sandbox
    .stub(ASRouterTelemetry.prototype, "getOrCreateImpressionId")
    .returns(FAKE_UUID);

  let instance = new ASRouterTelemetry();

  let data = {
    action: "cfr_user_event",
    event: "IMPRESSION",
    is_private: true,
    message_id: "cfr_message_01",
    bucket_id: "cfr_bucket_01",
  };
  let { ping, pingType } = await instance.applyCFRPolicy(data);

  Assert.equal(pingType, "cfr");
  Assert.equal(ping.impression_id, FAKE_UUID);
  Assert.equal(ping.client_id, undefined);
  Assert.equal(ping.bucket_id, "cfr_bucket_01");
  Assert.equal(ping.message_id, "n/a");

  sandbox.restore();
});

add_task(
  async function test_applyCFRPolicy_release_experiment_private_browsing() {
    info(
      "ASRouterTelemetry.applyCFRPolicy should use client_id and message_id in the " +
        "experiment cohort in Private Browsing in release"
    );
    let sandbox = sinon.createSandbox();
    sandbox.stub(UpdateUtils, "getUpdateChannel").returns("release");
    sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
      slug: "SOME-CFR-EXP",
    });

    let instance = new ASRouterTelemetry();

    let data = {
      action: "cfr_user_event",
      event: "IMPRESSION",
      is_private: true,
      message_id: "cfr_message_01",
      bucket_id: "cfr_bucket_01",
    };
    let { ping, pingType } = await instance.applyCFRPolicy(data);

    Assert.equal(pingType, "cfr");
    Assert.equal(ping.impression_id, undefined);
    Assert.equal(
      ping.client_id,
      Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
    );
    Assert.equal(ping.bucket_id, "cfr_bucket_01");
    Assert.equal(ping.message_id, "cfr_message_01");

    sandbox.restore();
  }
);

add_task(async function test_applyToolbarBadgePolicy() {
  info(
    "ASRouterTelemetry.applyToolbarBadgePolicy should set client_id and set pingType"
  );
  let instance = new ASRouterTelemetry();
  let { ping, pingType } = await instance.applyToolbarBadgePolicy({});

  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(pingType, "toolbar-badge");
});

add_task(async function test_applyInfoBarPolicy() {
  info(
    "ASRouterTelemetry.applyInfoBarPolicy should set client_id and set pingType"
  );
  let instance = new ASRouterTelemetry();
  let { ping, pingType } = await instance.applyInfoBarPolicy({});

  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(pingType, "infobar");
});

add_task(async function test_applyToastNotificationPolicy() {
  info(
    "ASRouterTelemetry.applyToastNotificationPolicy should set client_id " +
      "and set pingType"
  );
  let instance = new ASRouterTelemetry();
  let { ping, pingType } = await instance.applyToastNotificationPolicy({});

  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(pingType, "toast_notification");
});

add_task(async function test_applySpotlightPolicy() {
  info(
    "ASRouterTelemetry.applySpotlightPolicy should set client_id " +
      "and set pingType"
  );
  let instance = new ASRouterTelemetry();
  let { ping, pingType } = await instance.applySpotlightPolicy({
    action: "foo",
  });

  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(pingType, "spotlight");
  Assert.equal(ping.action, undefined);
});

add_task(async function test_applyMomentsPolicy_prerelease() {
  info(
    "ASRouterTelemetry.applyMomentsPolicy should use client_id and " +
      "message_id in prerelease"
  );
  let sandbox = sinon.createSandbox();
  sandbox.stub(UpdateUtils, "getUpdateChannel").returns("nightly");

  let instance = new ASRouterTelemetry();
  let data = {
    action: "moments_user_event",
    event: "IMPRESSION",
    message_id: "moments_message_01",
    bucket_id: "moments_bucket_01",
  };
  let { ping, pingType } = await instance.applyMomentsPolicy(data);

  Assert.equal(pingType, "moments");
  Assert.equal(ping.impression_id, undefined);
  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(ping.bucket_id, "moments_bucket_01");
  Assert.equal(ping.message_id, "moments_message_01");

  sandbox.restore();
});

add_task(async function test_applyMomentsPolicy_release() {
  info(
    "ASRouterTelemetry.applyMomentsPolicy should use impression_id and " +
      "bucket_id in release"
  );
  let sandbox = sinon.createSandbox();
  sandbox.stub(UpdateUtils, "getUpdateChannel").returns("release");
  sandbox
    .stub(ASRouterTelemetry.prototype, "getOrCreateImpressionId")
    .returns(FAKE_UUID);

  let instance = new ASRouterTelemetry();
  let data = {
    action: "moments_user_event",
    event: "IMPRESSION",
    message_id: "moments_message_01",
    bucket_id: "moments_bucket_01",
  };
  let { ping, pingType } = await instance.applyMomentsPolicy(data);

  Assert.equal(pingType, "moments");
  Assert.equal(ping.impression_id, FAKE_UUID);
  Assert.equal(ping.client_id, undefined);
  Assert.equal(ping.bucket_id, "moments_bucket_01");
  Assert.equal(ping.message_id, "n/a");

  sandbox.restore();
});

add_task(async function test_applyMomentsPolicy_experiment_release() {
  info(
    "ASRouterTelemetry.applyMomentsPolicy client_id and message_id in " +
      "the experiment cohort in release"
  );
  let sandbox = sinon.createSandbox();
  sandbox.stub(UpdateUtils, "getUpdateChannel").returns("release");
  sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
    slug: "SOME-CFR-EXP",
  });

  let instance = new ASRouterTelemetry();
  let data = {
    action: "moments_user_event",
    event: "IMPRESSION",
    message_id: "moments_message_01",
    bucket_id: "moments_bucket_01",
  };
  let { ping, pingType } = await instance.applyMomentsPolicy(data);

  Assert.equal(pingType, "moments");
  Assert.equal(ping.impression_id, undefined);
  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(ping.bucket_id, "moments_bucket_01");
  Assert.equal(ping.message_id, "moments_message_01");

  sandbox.restore();
});

add_task(async function test_applyMenuMessagePolicy() {
  info(
    "ASRouterTelemetry.applyMenuMessagePolicy should set client_id and set pingType"
  );
  let instance = new ASRouterTelemetry();
  let { ping, pingType } = await instance.applyMenuMessagePolicy({});

  Assert.equal(
    ping.client_id,
    Services.prefs.getCharPref("toolkit.telemetry.cachedClientID")
  );
  Assert.equal(pingType, "menu");
});

add_task(async function test_applyUndesiredEventPolicy() {
  info(
    "ASRouterTelemetry.applyUndesiredEventPolicy should exclude client_id " +
      "and use impression_id"
  );
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(ASRouterTelemetry.prototype, "getOrCreateImpressionId")
    .returns(FAKE_UUID);

  let instance = new ASRouterTelemetry();
  let data = {
    action: "asrouter_undesired_event",
    event: "RS_MISSING_DATA",
  };
  let { ping, pingType } = await instance.applyUndesiredEventPolicy(data);

  Assert.equal(pingType, "undesired-events");
  Assert.equal(ping.client_id, undefined);
  Assert.equal(ping.impression_id, FAKE_UUID);

  sandbox.restore();
});

add_task(async function test_createASRouterEvent_valid_ping() {
  info(
    "ASRouterTelemetry.createASRouterEvent should create a valid " +
      "ASRouterEventPing ping"
  );
  let instance = new ASRouterTelemetry();
  let action = {
    type: msg.AS_ROUTER_TELEMETRY_USER_EVENT,
    data: {
      action: "cfr_user_event",
      event: "CLICK",
      message_id: "cfr_message_01",
    },
  };
  let { ping } = await instance.createASRouterEvent(action);

  await assertASRouterEventPingValid(ping);
  Assert.equal(ping.event, "CLICK");
});

add_task(async function test_createASRouterEvent_call_correctPolicy() {
  let testCallCorrectPolicy = async (expectedPolicyFnName, data) => {
    info(
      `ASRouterTelemetry.createASRouterEvent should call ${expectedPolicyFnName} ` +
        `on action ${data.action} and event ${data.event}`
    );
    let sandbox = sinon.createSandbox();
    let instance = new ASRouterTelemetry();
    sandbox.stub(instance, expectedPolicyFnName);

    let action = { type: msg.AS_ROUTER_TELEMETRY_USER_EVENT, data };
    await instance.createASRouterEvent(action);
    Assert.ok(
      instance[expectedPolicyFnName].calledOnce,
      `ASRouterTelemetry.${expectedPolicyFnName} called`
    );

    sandbox.restore();
  };

  testCallCorrectPolicy("applyCFRPolicy", {
    action: "cfr_user_event",
    event: "IMPRESSION",
    message_id: "cfr_message_01",
  });

  testCallCorrectPolicy("applyToolbarBadgePolicy", {
    action: "badge_user_event",
    event: "IMPRESSION",
    message_id: "badge_message_01",
  });

  testCallCorrectPolicy("applyMomentsPolicy", {
    action: "moments_user_event",
    event: "CLICK_BUTTON",
    message_id: "moments_message_01",
  });

  testCallCorrectPolicy("applySpotlightPolicy", {
    action: "spotlight_user_event",
    event: "CLICK",
    message_id: "SPOTLIGHT_MESSAGE_93",
  });

  testCallCorrectPolicy("applyToastNotificationPolicy", {
    action: "toast_notification_user_event",
    event: "IMPRESSION",
    message_id: "TEST_TOAST_NOTIFICATION1",
  });

  testCallCorrectPolicy("applyUndesiredEventPolicy", {
    action: "asrouter_undesired_event",
    event: "UNDESIRED_EVENT",
  });
});

add_task(async function test_createASRouterEvent_stringify_event_context() {
  info(
    "ASRouterTelemetry.createASRouterEvent should stringify event_context if " +
      "it is an Object"
  );
  let instance = new ASRouterTelemetry();
  let action = {
    type: msg.AS_ROUTER_TELEMETRY_USER_EVENT,
    data: {
      action: "asrouter_undesired_event",
      event: "UNDESIRED_EVENT",
      event_context: { foo: "bar" },
    },
  };
  let { ping } = await instance.createASRouterEvent(action);

  Assert.equal(ping.event_context, JSON.stringify({ foo: "bar" }));
});

add_task(async function test_createASRouterEvent_not_stringify_event_context() {
  info(
    "ASRouterTelemetry.createASRouterEvent should not stringify event_context " +
      "if it is a String"
  );
  let instance = new ASRouterTelemetry();
  let action = {
    type: msg.AS_ROUTER_TELEMETRY_USER_EVENT,
    data: {
      action: "asrouter_undesired_event",
      event: "UNDESIRED_EVENT",
      event_context: "foo",
    },
  };
  let { ping } = await instance.createASRouterEvent(action);

  Assert.equal(ping.event_context, "foo");
});

add_task(async function test_onAction_calls_handleASRouterUserEvent() {
  let actions = [
    msg.AS_ROUTER_TELEMETRY_USER_EVENT,
    msg.TOOLBAR_BADGE_TELEMETRY,
    msg.TOOLBAR_PANEL_TELEMETRY,
    msg.MOMENTS_PAGE_TELEMETRY,
    msg.DOORHANGER_TELEMETRY,
  ];

  Services.prefs.setBoolPref(PREF_TELEMETRY, true);
  actions.forEach(type => {
    info(`Testing ${type} action`);
    let sandbox = sinon.createSandbox();
    let instance = new ASRouterTelemetry();

    const eventHandler = sandbox.spy(instance, "handleASRouterUserEvent");
    const action = {
      type,
      data: { event: "CLICK" },
    };

    instance.onAction(action);

    Assert.ok(eventHandler.calledWith(action));
    sandbox.restore();
  });

  Services.prefs.clearUserPref(PREF_TELEMETRY);
});

add_task(
  async function test_SendASRouterUndesiredEvent_calls_handleASRouterUserEvent() {
    info(
      "ASRouterTelemetry.SendASRouterUndesiredEvent should call " +
        "handleASRouterUserEvent"
    );
    let sandbox = sinon.createSandbox();
    let instance = new ASRouterTelemetry();

    sandbox.stub(instance, "handleASRouterUserEvent");

    instance.SendASRouterUndesiredEvent({ foo: "bar" });

    Assert.ok(
      instance.handleASRouterUserEvent.calledOnce,
      "ASRouterTelemetry.handleASRouterUserEvent was called once"
    );
    let [payload] = instance.handleASRouterUserEvent.firstCall.args;
    Assert.equal(payload.data.action, "asrouter_undesired_event");
    Assert.equal(payload.data.foo, "bar");

    sandbox.restore();
  }
);

add_task(
  async function test_handleASRouterUserEvent_calls_submitGleanPingForPing() {
    info(
      "ASRouterTelemetry.handleASRouterUserEvent should call " +
        "submitGleanPingForPing on known pingTypes when telemetry is enabled"
    );

    let data = {
      action: "spotlight_user_event",
      event: "IMPRESSION",
      message_id: "12345",
    };
    let sandbox = sinon.createSandbox();
    let instance = new ASRouterTelemetry();
    Services.fog.testResetFOG();

    Services.prefs.setBoolPref(PREF_TELEMETRY, true);

    sandbox.spy(AboutWelcomeTelemetry.prototype, "submitGleanPingForPing");

    await instance.handleASRouterUserEvent({ data });

    Assert.ok(
      AboutWelcomeTelemetry.prototype.submitGleanPingForPing.calledOnce,
      "AboutWelcomeTelemetry.submitGleanPingForPing called once"
    );

    Services.prefs.clearUserPref(PREF_TELEMETRY);
    sandbox.restore();
  }
);

add_task(
  async function test_handleASRouterUserEvent_no_submit_unknown_pingTypes() {
    info(
      "ASRouterTelemetry.handleASRouterUserEvent not submit pings on unknown pingTypes"
    );

    let data = {
      action: "unknown_event",
      event: "IMPRESSION",
      message_id: "12345",
    };
    let sandbox = sinon.createSandbox();
    let instance = new ASRouterTelemetry();
    Services.fog.testResetFOG();

    Services.prefs.setBoolPref(PREF_TELEMETRY, true);

    sandbox.spy(AboutWelcomeTelemetry.prototype, "submitGleanPingForPing");

    await instance.handleASRouterUserEvent({ data });

    Assert.ok(
      AboutWelcomeTelemetry.prototype.submitGleanPingForPing.notCalled,
      "AboutWelcomeTelemetry.submitGleanPingForPing not called"
    );

    Services.prefs.clearUserPref(PREF_TELEMETRY);
    sandbox.restore();
  }
);

add_task(
  async function test_isInCFRCohort_return_false_for_no_CFR_experiment() {
    info(
      "ASRouterTelemetry.isInCFRCohort should return false if there " +
        "is no CFR experiment registered"
    );
    let instance = new ASRouterTelemetry();
    Assert.ok(
      !instance.isInCFRCohort,
      "Should not be in CFR cohort by default"
    );
  }
);

add_task(
  async function test_isInCFRCohort_return_true_for_registered_CFR_experiment() {
    info(
      "ASRouterTelemetry.isInCFRCohort should return true if there " +
        "is a CFR experiment registered"
    );
    let sandbox = sinon.createSandbox();
    let instance = new ASRouterTelemetry();

    sandbox.stub(ExperimentAPI, "getExperimentMetaData").returns({
      slug: "SOME-CFR-EXP",
    });

    Assert.ok(instance.isInCFRCohort, "Should be in a CFR cohort");
    Assert.equal(
      ExperimentAPI.getExperimentMetaData.firstCall.args[0].featureId,
      "cfr"
    );

    sandbox.restore();
  }
);

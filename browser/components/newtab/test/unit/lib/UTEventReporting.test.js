import { UTSessionPing, UTUserEventPing } from "test/schemas/pings";
import { GlobalOverrider } from "test/unit/utils";
import { UTEventReporting } from "lib/UTEventReporting.sys.mjs";

const FAKE_EVENT_PING_PC = {
  event: "CLICK",
  source: "TOP_SITES",
  addon_version: "123",
  user_prefs: 63,
  session_id: "abc",
  page: "about:newtab",
  action_position: 5,
  locale: "en-US",
};
const FAKE_SESSION_PING_PC = {
  session_duration: 1234,
  addon_version: "123",
  user_prefs: 63,
  session_id: "abc",
  page: "about:newtab",
  locale: "en-US",
};
const FAKE_EVENT_PING_UT = [
  {
    value: "TOP_SITES",
    addon_version: "123",
    user_prefs: 63,
    session_id: "abc",
    page: "about:newtab",
    action_position: 5,
  },
];
const FAKE_SESSION_PING_UT = [
  {
    value: 1234,
    addon_version: "123",
    user_prefs: 63,
    session_id: "abc",
    page: "about:newtab",
  },
];

describe("UTEventReporting", () => {
  let globals;
  let sandbox;
  let utEvents;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    sandbox.stub(global.Services.telemetry, "setEventRecordingEnabled");

    utEvents = new UTEventReporting();
  });

  afterEach(() => {
    globals.restore();
  });

  describe("#sendUserEvent()", () => {
    it("should queue up the correct data to send to Events Telemetry", async () => {
      sandbox.stub(global.Glean.activityStream.eventClick, "record");
      utEvents.sendUserEvent(FAKE_EVENT_PING_PC);
      assert.calledWithExactly(
        global.Glean.activityStream.eventClick.record,
        ...FAKE_EVENT_PING_UT
      );

      let ping = global.Glean.activityStream.eventClick.record.firstCall.args;
      assert.validate(ping, UTUserEventPing);
    });
  });

  describe("#sendSessionEndEvent()", () => {
    it("should queue up the correct data to send to Events Telemetry", async () => {
      sandbox.stub(global.Glean.activityStream.endSession, "record");
      utEvents.sendSessionEndEvent(FAKE_SESSION_PING_PC);
      assert.calledWithExactly(
        global.Glean.activityStream.endSession.record,
        ...FAKE_SESSION_PING_UT
      );

      let ping = global.Glean.activityStream.endSession.record.firstCall.args;
      assert.validate(ping, UTSessionPing);
    });
  });

  describe("#uninit()", () => {
    it("should call setEventRecordingEnabled with a false value", () => {
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.firstCall.args[0],
        "activity_stream"
      );
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.firstCall.args[1],
        true
      );

      utEvents.uninit();
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.secondCall.args[0],
        "activity_stream"
      );
      assert.equal(
        global.Services.telemetry.setEventRecordingEnabled.secondCall.args[1],
        false
      );
    });
  });
});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that CA is active if and only if:
// 1. browser.contentanalysis.enabled is true and
// 2. Either browser.contentanalysis.enabled was set by an enteprise
//    policy or the "-allow-content-analysis" command line arg was present
// We can't really test command line arguments so we instead use a test-only
// method to set the value the command-line is supposed to update.

"use strict";

const { EnterprisePolicyTesting, PoliciesPrefTracker } =
  ChromeUtils.importESModule(
    "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
  );

const kIndividualPrefs = new Map([
  ["Enabled", "enabled"],
  ["PipeName", "pipe_path_name"],
  ["Timeout", "agent_timeout"],
  ["AllowUrl", "allow_url_regex_list"],
  ["DenyUrl", "deny_url_regex_list"],
  ["AgentName", "agent_name"],
  ["ClientSignature", "client_signature"],
  ["PerUser", "is_per_user"],
  ["MaxConnectionsCount", "max_connections"],
  ["ShowBlocked", "show_blocked_result"],
  ["DefaultResult", "default_result"],
  ["TimeoutResult", "timeout_result"],
  ["BypassForSameTab", "bypass_for_same_tab_operations"],
]);
function getIndividualPrefName(name) {
  is(
    kIndividualPrefs.has(name),
    true,
    `"${name}" passed to getIndividualPrefName() is valid`
  );
  return `browser.contentanalysis.${kIndividualPrefs.get(name)}`;
}
const kInterceptionPoints = [
  "clipboard",
  "download",
  "drag_and_drop",
  "file_upload",
  "print",
];
const kInterceptionPointsPlainTextOnly = ["clipboard", "drag_and_drop"];

const ca = Cc["@mozilla.org/contentanalysis;1"].getService(
  Ci.nsIContentAnalysis
);

add_task(async function test_ca_active() {
  PoliciesPrefTracker.start();
  ok(!ca.isActive, "CA is inactive when pref and cmd line arg are missing");

  // Set the pref without enterprise policy.  CA should not be active.
  Services.prefs.setBoolPref(getIndividualPrefName("Enabled"), true);
  ok(
    !ca.isActive,
    "CA is inactive when pref is set but cmd line arg is missing"
  );

  // Set the pref without enterprise policy but also set command line arg
  // property.  CA should be active.
  ca.testOnlySetCACmdLineArg(true);
  ok(ca.isActive, "CA is active when pref is set and cmd line arg is present");

  // Undo test-only value before later tests.
  ca.testOnlySetCACmdLineArg(false);
  ok(!ca.isActive, "properly unset cmd line arg value");

  // Disabled the pref with enterprise policy.  CA should not be active.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ContentAnalysis: { Enabled: false },
    },
  });
  ok(!ca.isActive, "CA is inactive when disabled by enterprise policy pref");

  // Enabled the pref with enterprise policy.  CA should be active.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ContentAnalysis: { Enabled: true },
    },
  });
  ok(ca.isActive, "CA is active when enabled by enterprise policy pref");
  for (let interceptionPoint of kInterceptionPoints) {
    const shouldBeEnabledByDefault = interceptionPoint !== "download";
    is(
      Services.prefs.getBoolPref(
        `browser.contentanalysis.interception_point.${interceptionPoint}.enabled`
      ),
      shouldBeEnabledByDefault,
      `${interceptionPoint} ${shouldBeEnabledByDefault ? "enabled" : "disabled"} by default`
    );
  }
  for (let interceptionPoint of kInterceptionPointsPlainTextOnly) {
    is(
      Services.prefs.getBoolPref(
        `browser.contentanalysis.interception_point.${interceptionPoint}.plain_text_only`
      ),
      true,
      `${interceptionPoint} plain_text_only on by default`
    );
  }

  Services.prefs.setBoolPref(getIndividualPrefName("Enabled"), false);
  PoliciesPrefTracker.stop();
});

add_task(async function test_ca_enterprise_config() {
  PoliciesPrefTracker.start();
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ContentAnalysis: {
        Enabled: true,
      },
    },
  });

  for (let individualPref of kIndividualPrefs.values()) {
    is(
      Services.prefs.prefIsLocked("browser.contentanalysis." + individualPref),
      true,
      `${individualPref} should be locked`
    );
  }

  for (let interceptionPoint of kInterceptionPoints) {
    is(
      Services.prefs.prefIsLocked(
        `browser.contentanalysis.interception_point.${interceptionPoint}.enabled`
      ),
      true,
      `${interceptionPoint} enabled should be locked`
    );
  }
  for (let interceptionPointPlainText of kInterceptionPointsPlainTextOnly) {
    is(
      Services.prefs.prefIsLocked(
        `browser.contentanalysis.interception_point.${interceptionPointPlainText}.plain_text_only`
      ),
      true,
      `${interceptionPointPlainText} plain_text_only should be locked`
    );
  }
  PoliciesPrefTracker.stop();
});

add_task(async function test_ca_enterprise_config() {
  PoliciesPrefTracker.start();
  const string1 = "this is a string";
  const string2 = "this is another string";
  const string3 = "an agent name";
  const string4 = "a client signature";

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ContentAnalysis: {
        PipePathName: "abc",
        AgentTimeout: 99,
        AllowUrlRegexList: string1,
        DenyUrlRegexList: string2,
        AgentName: string3,
        ClientSignature: string4,
        IsPerUser: true,
        MaxConnectionsCount: 3,
        ShowBlockedResult: false,
        DefaultResult: 1,
        TimeoutResult: 2,
        BypassForSameTabOperations: true,
        InterceptionPoints: {
          Clipboard: {
            Enabled: false,
            PlainTextOnly: false,
          },
          Download: {
            Enabled: true,
          },
          DragAndDrop: {
            Enabled: false,
            PlainTextOnly: false,
          },
          FileUpload: {
            Enabled: false,
          },
          Print: {
            Enabled: false,
          },
        },
      },
    },
  });

  is(
    Services.prefs.getStringPref(getIndividualPrefName("PipeName")),
    "abc",
    "pipe name match"
  );
  is(
    Services.prefs.getIntPref(getIndividualPrefName("Timeout")),
    99,
    "timeout match"
  );
  is(
    Services.prefs.getStringPref(getIndividualPrefName("AllowUrl")),
    string1,
    "allow urls match"
  );
  is(
    Services.prefs.getStringPref(getIndividualPrefName("DenyUrl")),
    string2,
    "deny urls match"
  );
  is(
    Services.prefs.getStringPref(getIndividualPrefName("AgentName")),
    string3,
    "agent names match"
  );
  is(
    Services.prefs.getStringPref(getIndividualPrefName("ClientSignature")),
    string4,
    "client signatures match"
  );
  is(
    Services.prefs.getIntPref(getIndividualPrefName("MaxConnectionsCount")),
    3,
    "connections count match"
  );
  is(
    Services.prefs.getBoolPref(getIndividualPrefName("PerUser")),
    true,
    "per user match"
  );
  is(
    Services.prefs.getBoolPref(getIndividualPrefName("ShowBlocked")),
    false,
    "show blocked match"
  );
  is(
    Services.prefs.getIntPref(getIndividualPrefName("DefaultResult")),
    1,
    "default result match"
  );
  is(
    Services.prefs.getIntPref(getIndividualPrefName("TimeoutResult")),
    2,
    "timeout result match"
  );
  is(
    Services.prefs.getBoolPref(getIndividualPrefName("BypassForSameTab")),
    true,
    "bypass for same tab operations match"
  );
  for (let interceptionPoint of kInterceptionPoints) {
    is(
      Services.prefs.getBoolPref(
        `browser.contentanalysis.interception_point.${interceptionPoint}.enabled`
      ),
      interceptionPoint === "download",
      `${interceptionPoint} interception point match`
    );
  }
  for (let interceptionPoint of kInterceptionPointsPlainTextOnly) {
    is(
      Services.prefs.getBoolPref(
        `browser.contentanalysis.interception_point.${interceptionPoint}.plain_text_only`
      ),
      false,
      `${interceptionPoint} interception point plain_text_only match`
    );
  }

  PoliciesPrefTracker.stop();
});

add_task(async function test_cleanup() {
  ca.testOnlySetCACmdLineArg(false);
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {},
  });
  // These may have gotten set when ContentAnalysis was enabled through
  // the policy and do not get cleared if there is no ContentAnalysis
  // element - reset them manually here.
  ca.isSetByEnterprisePolicy = false;
  Services.prefs.unlockPref(getIndividualPrefName("Enabled"));
  Services.prefs.clearUserPref(getIndividualPrefName("Enabled"));
  for (let interceptionPoint of kInterceptionPoints) {
    const prefName = `browser.contentanalysis.interception_point.${interceptionPoint}.enabled`;
    Services.prefs.unlockPref(prefName);
    Services.prefs.clearUserPref(prefName);
  }
  for (let interceptionPoint of kInterceptionPointsPlainTextOnly) {
    const prefName = `browser.contentanalysis.interception_point.${interceptionPoint}.plain_text_only`;
    Services.prefs.unlockPref(prefName);
    Services.prefs.clearUserPref(prefName);
  }
});

"use strict";

add_setup(async function () {
  // We don't send events or call official addon APIs while running
  // these tests, so there a good chance that test-verify mode may
  // end up seeing the addon as "idle". This pref should avoid that.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.background.idle.timeout", 300_000]],
  });
});

async function setupTestIntervention(interventions) {
  const config = {
    id: "test",
    label: "test intervention",
    bugs: {
      issue1: {
        matches: ["*://example.com/*"],
      },
    },
    interventions: interventions.map(i =>
      Object.assign({ platforms: ["all"], ua_string: ["Chrome"] }, i)
    ),
  };

  return await WebCompatExtension.updateInterventions([config]);
}

add_task(async function test_pref_check() {
  await WebCompatExtension.started();

  const checkableGlobalPrefs = WebCompatExtension.getCheckableGlobalPrefs();
  ok(
    checkableGlobalPrefs.includes("webcompat.test.pref1"),
    "allowed to access pref webcompat.test.pref1"
  );
  ok(
    checkableGlobalPrefs.includes("webcompat.test.pref2"),
    "allowed to access pref webcompat.test.pref2"
  );

  let [{ interventions }] = await setupTestIntervention([
    {
      pref_check: {
        "webcompat.test.pref1": false,
        "webcompat.test.pref2": false,
      },
    },
    {
      pref_check: {
        "webcompat.test.pref1": false,
        "webcompat.test.pref2": true,
      },
    },
    {
      pref_check: {
        "webcompat.test.pref1": true,
        "webcompat.test.pref2": false,
      },
    },
    {
      pref_check: {
        "webcompat.test.pref1": true,
        "webcompat.test.pref2": true,
      },
    },
  ]);

  ok(
    !interventions[0].enabled,
    "intervention 1 should NOT be enabled (prefs still undefined)"
  );
  ok(
    !interventions[1].enabled,
    "intervention 2 should NOT be enabled (prefs still undefined)"
  );
  ok(
    !interventions[2].enabled,
    "intervention 3 should NOT be enabled (prefs still undefined)"
  );
  ok(
    !interventions[3].enabled,
    "intervention 4 should NOT be enabled (prefs still undefined)"
  );

  Services.prefs.setBoolPref("webcompat.test.pref1", true);
  Services.prefs.setBoolPref("webcompat.test.pref2", false);
  await WebCompatExtension.noOngoingInterventionChanges();
  interventions = (await WebCompatExtension.getInterventionById("test"))
    .interventions;

  ok(
    !interventions[0].enabled,
    "intervention 1 should NOT be enabled (test1=true, test2=false)"
  );
  ok(
    !interventions[1].enabled,
    "intervention 2 should NOT be enabled (test1=true, test2=false)"
  );
  ok(
    interventions[2].enabled,
    "intervention 3 should be enabled     (test1=true, test2=false)"
  );
  ok(
    !interventions[3].enabled,
    "intervention 4 should NOT be enabled (test1=true, test2=false)"
  );

  Services.prefs.setBoolPref("webcompat.test.pref1", false);
  Services.prefs.setBoolPref("webcompat.test.pref2", true);
  await WebCompatExtension.noOngoingInterventionChanges();
  interventions = (await WebCompatExtension.getInterventionById("test"))
    .interventions;

  ok(
    !interventions[0].enabled,
    "intervention 1 should NOT be enabled (test1=false, test2=true)"
  );
  ok(
    interventions[1].enabled,
    "intervention 2 should be enabled     (test1=false, test2=true)"
  );
  ok(
    !interventions[2].enabled,
    "intervention 3 should NOT be enabled (test1=false, test2=true)"
  );
  ok(
    !interventions[3].enabled,
    "intervention 4 should NOT be enabled (test1=false, test2=true)"
  );

  Services.prefs.clearUserPref("webcompat.test.pref1");
  Services.prefs.clearUserPref("webcompat.test.pref2");
  await WebCompatExtension.noOngoingInterventionChanges();
  interventions = (await WebCompatExtension.getInterventionById("test"))
    .interventions;

  ok(
    !interventions[0].enabled,
    "intervention 1 should NOT be enabled (prefs cleared)"
  );
  ok(
    !interventions[1].enabled,
    "intervention 2 should NOT be enabled (prefs cleared)"
  );
  ok(
    !interventions[2].enabled,
    "intervention 3 should NOT be enabled (prefs cleared)"
  );
  ok(
    !interventions[3].enabled,
    "intervention 4 should NOT be enabled (prefs cleared)"
  );
});

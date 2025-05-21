"use strict";

add_setup(async function () {
  // We don't send events or call official addon APIs while running
  // these tests, so there a good chance that test-verify mode may
  // end up seeing the addon as "idle". This pref should avoid that.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.background.idle.timeout", 300_000]],
  });
});

function getConfig(id, interventions) {
  return {
    id,
    label: id,
    bugs: {
      issue1: {
        matches: ["*://example.com/*"],
      },
    },
    interventions: interventions.map(i => {
      if (!i.platforms) {
        i.platforms = "all";
      }
      const { css, js } = i;
      delete i.css;
      delete i.js;
      i.content_scripts = { css, js };
      return i;
    }),
  };
}

add_task(async function test_that_only_intended_interventions_are_activated() {
  const runningVersion = parseInt(
    navigator.userAgent.match("Firefox/([0-9]*)")[1]
  );

  // If we have multiple parts to an intervention, where only some of them are meant to
  // apply (like ones for different Firefox versions), we want to make sure that only the
  // intended ones are activated. Otherwise when we try to deactivate them in about:compat,
  // it will fail, and we also run the risk of enabling more than we intended to.
  const config = getConfig("test", [
    {
      min_version: runningVersion + 1,
      js: ["lib/intervention_helpers.js"],
    },
    {
      max_version: runningVersion,
      js: ["lib/ua_helpers.js"],
    },
    {
      js: ["lib/shim_messaging_helper.js"],
    },
    {
      platforms: ["invalid"],
      js: ["lib/messaging_helper.js"],
    },
  ]);

  const [{ interventions }] = await WebCompatExtension.updateInterventions([
    config,
  ]);
  Assert.deepEqual(
    interventions.map(i => i.enabled),
    [false, true, true, false],
    "The correct parts of the intervention were chosen to be enabled"
  );

  let reg = await WebCompatExtension.getRegisteredContentScriptsFor("test");
  Assert.deepEqual(
    reg.map(r => r.js).flat(),
    ["lib/ua_helpers.js", "lib/shim_messaging_helper.js"],
    "Content scripts were properly registered"
  );

  await WebCompatExtension.disableInterventions(["test"]);

  reg = await WebCompatExtension.getRegisteredContentScriptsFor(["test"]);
  Assert.deepEqual(
    reg.map(r => r.js).flat(),
    [],
    "Content scripts were properly unregistered"
  );
});

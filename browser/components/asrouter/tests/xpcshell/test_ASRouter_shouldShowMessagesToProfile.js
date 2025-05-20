/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { ASRouter } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouter.sys.mjs"
);

const { ASRouterTargeting } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterTargeting.sys.mjs"
);

add_task(async function test_shouldShowMessagesToProfile() {
  let sandbox = sinon.createSandbox();
  // shouldShowMessages should return true if the Selectable Profile Service is not enabled
  Services.prefs.setBoolPref("browser.profiles.enabled", false);

  Assert.equal(ASRouter.shouldShowMessagesToProfile(), true);
  // should return true if the Selectable Profile Service is enabled but no profiles have been created
  Services.prefs.setBoolPref("browser.profiles.enabled", true);

  Assert.equal(ASRouter.shouldShowMessagesToProfile(), true);
  // should return false if the Selectable Profile Service is enabled, and there is a profile but the profile IDs don't match
  await initSelectableProfileService();
  Services.prefs.setBoolPref("browser.profiles.created", true);
  Services.prefs.setStringPref(
    "messaging-system.profile.messagingProfileId",
    "2"
  );
  sandbox.replaceGetter(
    ASRouterTargeting.Environment,
    "currentProfileId",
    function () {
      return "1";
    }
  );
  Assert.equal(ASRouter.shouldShowMessagesToProfile(), false);
  // should return true if the Selectable Profile Service is enabled, and the profile IDs match
  Services.prefs.setStringPref(
    "messaging-system.profile.messagingProfileId",
    "1"
  );
  Assert.equal(ASRouter.shouldShowMessagesToProfile(), true);
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { ASRouterPreferences } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterPreferences.sys.mjs"
);

add_task(async function test_maybeSetMessagingProfileID() {
  await initSelectableProfileService();
  let currentProfile = sinon
    .stub(SelectableProfileService, "currentProfile")
    .value({ id: 1 });
  sinon.stub(SelectableProfileService, "trackPref").resolves();

  // If the Profile ID pref is unset and a profile exists, set it
  Services.prefs.setStringPref(
    "messaging-system.profile.messagingProfileId",
    ""
  );
  await ASRouterPreferences._maybeSetMessagingProfileID();

  Assert.equal(
    "1",
    Services.prefs.getStringPref("messaging-system.profile.messagingProfileId")
  );

  // Once the ID has been set, check to see if a profile exists
  currentProfile.value({ id: 2 });
  let messagingProfile = sinon
    .stub(SelectableProfileService, "getProfile")
    .returns({ id: 1 });
  // If the profile exists, do nothing
  await ASRouterPreferences._maybeSetMessagingProfileID();

  Assert.equal(
    "1",
    Services.prefs.getStringPref("messaging-system.profile.messagingProfileId")
  );
  // If the profile does not exist, reset the Profile ID pref
  messagingProfile.returns(null);

  await ASRouterPreferences._maybeSetMessagingProfileID();

  Assert.equal(
    "2",
    Services.prefs.getStringPref("messaging-system.profile.messagingProfileId")
  );
});

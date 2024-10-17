/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_CREATE_NEW_SELECTABLE_PROFILE() {
  let profileStub = sinon.stub(SpecialMessageActions, "createAndOpenProfile");

  await SMATestUtils.executeAndValidateAction({
    type: "CREATE_NEW_SELECTABLE_PROFILE",
  });

  Assert.equal(
    profileStub.callCount,
    1,
    "createAndOpenProfile called by the action"
  );

  profileStub.restore();
});

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormAutofillUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillUtils.sys.mjs"
);

// Check whether os auth is disabled by default for new profiles
add_task(async function test_creditCards_os_auth_disabled_for_new_profile() {
  Assert.ok(
    !FormAutofillUtils.getOSAuthEnabled(),
    "OS Auth should be disabled for credit cards by default for a new profile."
  );

  Assert.ok(
    Services.prefs.prefIsLocked(
      FormAutofillUtils.AUTOFILL_CREDITCARDS_OS_AUTH_LOCKED_PREF
    ),
    "Pref should be locked"
  );

  Assert.ok(
    !LoginHelper.getOSAuthEnabled(),
    "OS Auth should be disabled for passwords by default."
  );

  Assert.ok(
    Services.prefs.prefIsLocked(LoginHelper.OS_AUTH_FOR_PASSWORDS_BOOL_PREF),
    "Pref should be locked"
  );
});

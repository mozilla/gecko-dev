/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_STRING_NO_DEFAULT = "test.aboutconfig.a";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_STRING_NO_DEFAULT, "some value"]],
  });
});

add_task(async function test_locked() {
  registerCleanupFunction(() => {
    Services.prefs.unlockPref(PREF_STRING_DEFAULT_NOTEMPTY);
    Services.prefs.unlockPref(PREF_BOOLEAN_DEFAULT_TRUE);
    Services.prefs.unlockPref(PREF_STRING_NO_DEFAULT);
  });

  Services.prefs.lockPref(PREF_STRING_DEFAULT_NOTEMPTY);
  Services.prefs.lockPref(PREF_BOOLEAN_DEFAULT_TRUE);
  Services.prefs.lockPref(PREF_STRING_NO_DEFAULT);

  await AboutConfigTest.withNewTab(async function () {
    let click = (target, opts) =>
      EventUtils.synthesizeMouseAtCenter(target, opts, this.window);
    let doubleClick = target => {
      // We intentionally turn off this a11y check, because the following series
      // of clicks (in these test cases) is either performing an activation of
      // the edit mode for prefs or selecting a text in focused inputs. The
      // edit mode can be activated with a separate "Edit" or "Toggle" button
      // provided for each pref, and the text selection can be performed with
      // caret browsing (when supported). Thus, this rule check can be ignored
      // by a11y_checks suite.
      AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
      // Trigger two mouse events to simulate the first then second click.
      click(target, { clickCount: 1 });
      click(target, { clickCount: 2 });
      AccessibilityUtils.resetEnv();
    };
    // Test locked default string pref.
    let lockedPref = this.getRow(PREF_STRING_DEFAULT_NOTEMPTY);
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, PREF_STRING_DEFAULT_NOTEMPTY_VALUE);
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-edit"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test locked default boolean pref.
    lockedPref = this.getRow(PREF_BOOLEAN_DEFAULT_TRUE);
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, "true");
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-toggle"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    doubleClick(lockedPref.valueCell);
    Assert.equal(lockedPref.value, "true");
    Services.prefs.unlockPref(PREF_BOOLEAN_DEFAULT_TRUE);
    Assert.equal(lockedPref.value, "true");

    // Test locked user added pref.
    lockedPref = this.getRow(PREF_STRING_NO_DEFAULT);
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, "");
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-edit"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test pref not locked.
    let unlockedPref = this.getRow(PREF_BOOLEAN_USERVALUE_TRUE);
    Assert.ok(!unlockedPref.hasClass("locked"));
    Assert.equal(unlockedPref.value, "true");
    Assert.ok(
      unlockedPref.editColumnButton.classList.contains("button-toggle")
    );
    Assert.ok(!unlockedPref.editColumnButton.disabled);
  });
});

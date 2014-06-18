/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test_locale() {
  // Set the pref to something controlled.
  Services.prefs.setCharPref("general.useragent.locale", "ab-CD");

  Assert.equal(MozLoopService.locale, "ab-CD");

  Services.prefs.clearUserPref("general.useragent.locale");
}

function test_getStrings() {
  // Try an invalid string
  Assert.equal(MozLoopService.getStrings("invalid_not_found_string"), "");

  // Get a string that has sub-items to test the function more fully.
  // XXX This depends on the L10n values, which I'd prefer not to do, but is the
  // simplest way for now.
  Assert.equal(MozLoopService.getStrings("caller"), '{"placeholder":"Identify this call"}');
}

function run_test()
{
  test_locale();
  test_getStrings();
}

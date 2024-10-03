/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kEnabledPref = "accessibility.typeaheadfind.enablesound";
const kSoundURLPref = "accessibility.typeaheadfind.soundURL";
const kClassicSoundURL = "chrome://global/content/notfound.wav";

const { resetSound, playNotFoundSound } = ChromeUtils.importESModule(
  "resource://gre/modules/FinderSound.sys.mjs"
);

const MockSound = SpecialPowers.MockSound;

add_setup(() => {
  MockSound.init();
  resetSound();
  registerCleanupFunction(() => MockSound.cleanup());
});

add_task(async function test_notfound_sound_with_preferences() {
  await SpecialPowers.pushPrefEnv({
    set: [[kSoundURLPref, "beep"]],
  });
  MockSound.reset();
  playNotFoundSound();
  SimpleTest.isDeeply(MockSound.played, ["beep"], '"beep" notfound sound');

  await SpecialPowers.pushPrefEnv({
    set: [[kSoundURLPref, "default"]],
  });
  MockSound.reset();
  playNotFoundSound();
  SimpleTest.isDeeply(
    MockSound.played,
    [`(uri)${kClassicSoundURL}`],
    '"default" notfound sound is a bulit-in wav'
  );

  await SpecialPowers.pushPrefEnv({
    set: [[kSoundURLPref, ""]],
  });
  MockSound.reset();
  playNotFoundSound();
  SimpleTest.isDeeply(
    MockSound.played,
    [],
    "Empty notfound sound plays nothing"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      [kSoundURLPref, "beep"],
      [kEnabledPref, false],
    ],
  });
  MockSound.reset();
  playNotFoundSound();
  SimpleTest.isDeeply(MockSound.played, [], "Disable sound completely");
});

/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kEnabledPref = "accessibility.typeaheadfind.enablesound";
const kSoundURLPref = "accessibility.typeaheadfind.soundURL";
const kWrappedSoundURLPref = "accessibility.typeaheadfind.wrappedSoundURL";
const kClassicSoundURL = "chrome://global/content/notfound.wav";

const { resetSound, playSound } = ChromeUtils.importESModule(
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
    set: [
      [kSoundURLPref, "beep"],
      [kWrappedSoundURLPref, ""],
      [kEnabledPref, true],
    ],
  }); // default value

  MockSound.reset();
  playSound("not-found");
  SimpleTest.isDeeply(MockSound.played, ["beep"], '"beep" notfound sound');

  await SpecialPowers.pushPrefEnv({
    set: [[kSoundURLPref, "default"]],
  });
  MockSound.reset();
  playSound("not-found");
  SimpleTest.isDeeply(
    MockSound.played,
    [`(uri)${kClassicSoundURL}`],
    '"default" notfound sound is a bulit-in wav'
  );

  await SpecialPowers.pushPrefEnv({
    set: [[kSoundURLPref, ""]],
  });
  MockSound.reset();
  playSound("not-found");
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
  playSound("not-found");
  SimpleTest.isDeeply(
    MockSound.played,
    [],
    "Disable sound completely (testing: not-found)"
  );
});

add_task(async function test_wrapped_sound_with_preferences() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [kSoundURLPref, "beep"],
      [kWrappedSoundURLPref, ""],
      [kEnabledPref, true],
    ],
  }); // default value

  MockSound.reset();
  playSound("wrapped");
  SimpleTest.isDeeply(MockSound.played, [], "No wrapped sound by default");

  await SpecialPowers.pushPrefEnv({
    set: [[kWrappedSoundURLPref, "beep"]],
  });
  MockSound.reset();
  playSound("wrapped");
  SimpleTest.isDeeply(MockSound.played, ["beep"], '"beep" wrapped sound');

  await SpecialPowers.pushPrefEnv({
    set: [[kWrappedSoundURLPref, "default"]],
  });
  MockSound.reset();
  playSound("wrapped");
  SimpleTest.isDeeply(
    MockSound.played,
    [`(uri)${kClassicSoundURL}`],
    '"default" wrapped sound is a bulit-in wav'
  );

  await SpecialPowers.pushPrefEnv({
    set: [[kWrappedSoundURLPref, ""]],
  });
  MockSound.reset();
  playSound("wrapped");
  SimpleTest.isDeeply(
    MockSound.played,
    [],
    "Empty wrapped sound plays nothing"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      [kWrappedSoundURLPref, "beep"],
      [kEnabledPref, false],
    ],
  });
  MockSound.reset();
  playSound("wrapped");
  SimpleTest.isDeeply(
    MockSound.played,
    [],
    "Disable sound completely (testing: wrapped)"
  );
});

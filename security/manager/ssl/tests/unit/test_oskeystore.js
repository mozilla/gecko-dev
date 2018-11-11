/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests the methods and attributes for interfacing with nsIOSKeyStore.

// Ensure that the appropriate initialization has happened.
do_get_profile();

const LABELS = ["mylabel1",
                "mylabel2",
                "mylabel3"];

async function delete_all_secrets() {
  let keystore = Cc["@mozilla.org/security/oskeystore;1"]
                   .getService(Ci.nsIOSKeyStore);
  for (let label of LABELS) {
    if (await keystore.asyncSecretAvailable(label)) {
      await keystore.asyncDeleteSecret(label);
      ok(!await keystore.asyncSecretAvailable(label), label + " should be deleted now.");
    }
  }
}

// Test that Firefox handles locking and unlocking of the OSKeyStore properly.
// Does so by mocking out the actual dialog and "filling in" the
// password. Also tests that providing an incorrect password will fail (well,
// technically the user will just get prompted again, but if they then cancel
// the dialog the overall operation will fail).

var gMockPrompter = {
  passwordToTry: null,
  numPrompts: 0,

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gMockPrompter| due to
  // how objects get wrapped when going across xpcom boundaries.
  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    this.numPrompts++;
    if (this.numPrompts > 1) { // don't keep retrying a bad password
      return false;
    }
    equal(text,
          "Please enter your master password.",
          "password prompt text should be as expected");
    equal(checkMsg, null, "checkMsg should be null");
    ok(this.passwordToTry, "passwordToTry should be non-null");
    password.value = this.passwordToTry;
    return true;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
};

// Mock nsIWindowWatcher. PSM calls getNewPrompter on this to get an nsIPrompt
// to call promptPassword. We return the mock one, above.
var gWindowWatcher = {
  getNewPrompter: () => gMockPrompter,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowWatcher]),
};

async function encrypt_decrypt_test() {
  let keystore = Cc["@mozilla.org/security/oskeystore;1"]
                   .getService(Ci.nsIOSKeyStore);
  ok(!await keystore.asyncSecretAvailable(LABELS[0]), "The secret should not be available yet.");

  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(recoveryPhrase, "A recovery phrase should've been created.");
  let recoveryPhrase2 = await keystore.asyncGenerateSecret(LABELS[1]);
  ok(recoveryPhrase2, "A recovery phrase should've been created.");

  let text = new Uint8Array([0x01, 0x00, 0x01]);
  let ciphertext = "";
  try {
    ciphertext = await keystore.asyncEncryptBytes(LABELS[0], text.length, text);
    ok(ciphertext, "We should have a ciphertext now.");
  } catch (e) {
    ok(false, "Error encrypting " + e);
  }

  // Decrypting should give us the plaintext bytes again.
  try {
    let plaintext = await keystore.asyncDecryptBytes(LABELS[0], ciphertext);
    ok(plaintext.toString() == text.toString(), "Decrypted plaintext should be the same as text.");
  } catch (e) {
    ok(false, "Error decrypting ciphertext " + e);
  }

  // Decrypting with a wrong key should throw an error.
  try {
    await keystore.asyncDecryptBytes(LABELS[1], ciphertext);
    ok(false, "Decrypting with the wrong key should fail.");
  } catch (e) {
    ok(true, "Decrypting with the wrong key should fail " + e);
  }
}

add_task(async function() {
  let keystore = Cc["@mozilla.org/security/oskeystore;1"]
                   .getService(Ci.nsIOSKeyStore);
  let windowWatcherCID;
  if (keystore.isNSSKeyStore) {
    windowWatcherCID =
      MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                             gWindowWatcher);
    registerCleanupFunction(() => {
      MockRegistrar.unregister(windowWatcherCID);
    });
  }

  await delete_all_secrets();
  await encrypt_decrypt_test();
  await delete_all_secrets();

  if (AppConstants.platform == "macosx" || AppConstants.platform == "win") {
    ok(!keystore.isNSSKeyStore, "OS X and Windows should use the non-NSS implementation");
  }

  if (keystore.isNSSKeyStore) {
    // If we use the NSS key store implementation test that everything works
    // when a master password is set.
    // Set an initial password.
    let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                    .getService(Ci.nsIPK11TokenDB);
    let token = tokenDB.getInternalKeyToken();
    token.initPassword("hunter2");

    // Lock the key store. This should be equivalent to token.logoutSimple()
    await keystore.asyncLock();

    // Set the correct password so that the test operations should succeed.
    gMockPrompter.passwordToTry = "hunter2";
    await encrypt_decrypt_test();
    ok(gMockPrompter.numPrompts == 1, "There should've been one password prompt.");
    await delete_all_secrets();
  }
});

// Test that if we kick off a background operation and then call a synchronous function on the
// keystore, we don't deadlock.
add_task(async function() {
  await delete_all_secrets();

  let keystore = Cc["@mozilla.org/security/oskeystore;1"]
                   .getService(Ci.nsIOSKeyStore);
  let recoveryPhrase = await keystore.asyncGenerateSecret(LABELS[0]);
  ok(recoveryPhrase, "A recovery phrase should've been created.");

  try {
    let text = new Uint8Array(8192);
    let promise = keystore.asyncEncryptBytes(LABELS[0], text.length, text);
    /* eslint-disable no-unused-expressions */
    keystore.isNSSKeyStore; // we don't care what this is - we just need to access it
    /* eslint-enable no-unused-expressions */
    let ciphertext = await promise;
    ok(ciphertext, "We should have a ciphertext now.");
  } catch (e) {
    ok(false, "Error encrypting " + e);
  }

  await delete_all_secrets();
});

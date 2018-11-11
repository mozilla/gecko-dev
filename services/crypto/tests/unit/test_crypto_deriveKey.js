Components.utils.import("resource://services-crypto/WeaveCrypto.js");

function run_test() {
  let cryptoSvc = new WeaveCrypto();
  // Extracted from test_utils_deriveKey.
  let pp = "secret phrase";
  let salt = "RE5YUHpQcGl3bg==";   // btoa("DNXPzPpiwn")

  // 16-byte, extract key data.
  let k = cryptoSvc.deriveKeyFromPassphrase(pp, salt, 16);
  do_check_eq(16, k.length);
  do_check_eq(btoa(k), "d2zG0d2cBfXnRwMUGyMwyg==");

  // Test different key lengths.
  k = cryptoSvc.deriveKeyFromPassphrase(pp, salt, 32);
  do_check_eq(32, k.length);
  do_check_eq(btoa(k), "d2zG0d2cBfXnRwMUGyMwyroRXtnrSIeLwSDvReSfcyA=");
  let encKey = btoa(k);

  // Test via encryption.
  let iv = cryptoSvc.generateRandomIV();
  do_check_eq(cryptoSvc.decrypt(cryptoSvc.encrypt("bacon", encKey, iv), encKey, iv), "bacon");

  // Test default length (32).
  k = cryptoSvc.deriveKeyFromPassphrase(pp, salt);
  do_check_eq(32, k.length);
  do_check_eq(encKey, btoa(k));
}

try {
  new RegExp("[\\00]", "u");
} catch (e) {
  assertEq(e.message, "invalid decimal escape in regular expression");
}

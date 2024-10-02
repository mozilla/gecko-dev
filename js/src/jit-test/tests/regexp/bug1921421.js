// |jit-test| --enable-regexp-modifiers

try {
  new RegExp("(?--");
} catch (e) {
  assertEq(e.message, "multiple dashes in flag group");
}

var s = "";
function f() {
  ensureLinearString(s += "abcdefghijklm");
}
for (let i = 0; i < 8; i++) {
  oomTest(f);
}

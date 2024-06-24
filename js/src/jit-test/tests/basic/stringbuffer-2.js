var s = newString("abcdefg".repeat(5), {newStringBuffer: true});
for (var i = 0; i < 10; i++) {
  s = s.substring(1);
}
assertEq(s, "defgabcdefgabcdefgabcdefg");

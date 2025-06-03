// |str.slice(-1)| and |str.substr(-1)| are compiled as |str.at(str.length - 1)|.

const strings = [
  "",
  "a", "b",
  "ab", "ba",
];

for (let i = 0; i < 1000; ++i) {
  let str = strings[i % strings.length];

  assertEq(str.slice(-1), str.charAt(str.length - 1));
  assertEq(str.substr(-1), str.charAt(str.length - 1));
}

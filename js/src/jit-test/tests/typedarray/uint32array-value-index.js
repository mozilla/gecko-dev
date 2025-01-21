let values = [0, 0xff, 0x7fff_ffff, 0xffff_0000, 0x8000_0000, 0xffff_fffe, 0xffff_ffff];
let ta = new Uint32Array(values);
values.forEach((value, i) => {
  assertEq(ta.indexOf(value), i);
  assertEq(ta.indexOf(value + 2), -1);
  assertEq(ta.includes(value), true);
  assertEq(ta.includes(value + 0.0001), false);
  assertEq(ta.lastIndexOf(value), i);
  assertEq(ta.lastIndexOf(value + 3), -1);
});

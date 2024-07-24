for (let v0 = 0; v0 < 50; v0++) {
  class C1 {}
  new C1();
  eval();
  const s = '{ "phbbbbbbbbbbbbbbttt!!!!??": [1] }';
  const r1 = s.substr(0, 18) + s.substr(18);
  const flat1 = ensureLinearString(r1);
  const r2 = newRope(flat1, "\n");
  let r3 = newRope(r2, "toLowerCase");
  ensureLinearString(r3);
  r3 = null;
  JSON.parse(flat1);
}

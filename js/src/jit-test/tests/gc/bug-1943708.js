gczeal(11, 4);
gczeal(9, 1);
try { evaluate(`
  gcparam("maxBytes", gcparam("gcBytes") + 4*1024);
  assertEq();
`); } catch(exc) {}

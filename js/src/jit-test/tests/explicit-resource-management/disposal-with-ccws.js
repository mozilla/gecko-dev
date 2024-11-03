// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const g1 = newGlobal({ newCompartment: true });
  const d = g1.eval(`
    globalThis.disposed = [];
    const d = { [Symbol.dispose]: () => globalThis.disposed.push(1) };
    d;
  `);
  const g2 = newGlobal({ newCompartment: true });
  const d2 = g2.eval(`
    globalThis.disposed = [];
    const d2 = { [Symbol.dispose]: () => globalThis.disposed.push(2) };
    d2;
  `);
  {
    using disp = d;
    using disp2 = d2;
  }
  assertArrayEq(g1.eval("globalThis.disposed"), [1]);
  assertArrayEq(g2.eval("globalThis.disposed"), [2]);
}

{
  const g1 = newGlobal({ newCompartment: true });
  const d = g1.eval(`
    globalThis.disposed = [];
    const d = { [Symbol.dispose]: () => globalThis.disposed.push(1) };
    d;
  `);
  const g2 = newGlobal({ newCompartment: true });
  const d2 = g2.eval(`
    globalThis.disposed = [];
    const d2 = { [Symbol.dispose]: () => globalThis.disposed.push(2) };
    d2;
  `);
  assertThrowsInstanceOf(() => {
    {
      using disp = d;
      using disp2 = d2;
      nukeAllCCWs();
    }
  }, SuppressedError);
}

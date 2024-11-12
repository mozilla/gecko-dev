// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const g = newGlobal({ newCompartment: true });
  const d = g.eval(`
    globalThis.disposed = [];
    const d = new DisposableStack();
    d.use({ [Symbol.dispose]: () => globalThis.disposed.push(1) });
    d;
  `);
  {
    DisposableStack.prototype.dispose.call(d);
    assertEq(d.disposed, true);
    assertArrayEq(g.eval("globalThis.disposed"), [1]);
  }
}

{
  {
    const g = newGlobal({ newCompartment: true });
    const d = g.eval(`
      globalThis.disposed = [];
      const d = new DisposableStack();
      d.use({ [Symbol.dispose]: () => globalThis.disposed.push(1) });
      d;
    `);
    nukeAllCCWs();
    {
      assertThrowsInstanceOf(() => DisposableStack.prototype.dispose.call(d), TypeError);
    }
  }
}

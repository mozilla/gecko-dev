// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  };
  outer: for (let a of [1]) {
    for (using x of [d]) {
      {
        let a = 0, b = () => a;
        break outer;
      }
    }
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  };
  outer: for (let a of [1]) {
    for (using x of [d]) {
      continue outer;
    }
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  };
  outer: for (let a of [1]) {
    for (using x of [d]) {
      {
        let a = 0, b = () => a;
        continue outer;
      }
    }
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  }
  for (const i in { a: 1 }) {
    using x = d;
    break;
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  }
  outer: for (const i in { a: 1 }) {
    for (const j in { b: 1 }) {
      using x = d;
      break outer;
    }
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  }
  outer: for (const i in { a: 1 }) {
    for (const j in { b: 1 }) {
      using x = d;
      continue outer;
    }
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  }
  outer: for (const i in { a: 1 }) {
    for (const j in { b: 1 }) {
      using x = d;
      {
        let a = 0, b = () => a;
        break outer;
      }
    }
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const d = {
    [Symbol.dispose]() {
      disposed.push(1);
    }
  }
  outer: for (const i in { a: 1 }) {
    for (const j in { b: 1 }) {
      using x = d;
      {
        let a = 0, b = () => a;
        continue outer;
      }
    }
  }
  assertArrayEq(disposed, [1]);
}

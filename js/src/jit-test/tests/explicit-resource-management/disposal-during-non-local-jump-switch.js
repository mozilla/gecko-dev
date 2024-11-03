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
    switch (1) {
      case 1:
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
  };
  outer: for (let a of [1]) {
    switch (1) {
      case 1: {
        let a = 0, b = () => a;
        {
          using x = d;
          break outer;
        }
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
  switch (1) {
    case 1:
      using x = d;
      {
        let a = 0, b = () => a;
        break;
      }
  }
  assertArrayEq(disposed, [1]);
}

{
  const disposedBreak = [];
  const disposedContinue = [];

  function testSwitchWithNonLocalJumpOut(opt) {
    outer: for (let a of [1, 2]) {
      switch (opt) {
        case 'break':
          using x = {
            [Symbol.dispose]() {
              disposedBreak.push(1);
            }
          };
          break outer;
        case 'continue':
          using y = {
            [Symbol.dispose]() {
              disposedContinue.push(a);
            }
          };
          continue outer;
      }
    }
  }
  testSwitchWithNonLocalJumpOut('break');
  assertArrayEq(disposedBreak, [1]);
  testSwitchWithNonLocalJumpOut('continue');
  assertArrayEq(disposedContinue, [1, 2]);
}
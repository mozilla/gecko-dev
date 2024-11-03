// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  let disposed = [];

  function testDisposableWithThrowAndSwitch(toThrow) {
    try {
      using d = {
        [Symbol.dispose]() {
          disposed.push(1);
        }
      };
      switch (toThrow) {
        case 'break':
          using d2 = {
            [Symbol.dispose]() {
              disposed.push(2);
            }
          };
          break;
        case 'throw': {
          using d3 = {
            [Symbol.dispose]() {
              disposed.push(3);
            }
          }
          throw new Error("err");
        }
        case 'throw_with_extra_scope':
          using d4 = {
            [Symbol.dispose]() {
              disposed.push(4);
            }
          };
          {
            let a = 0, b = () => a;
            throw new Error("err");
          }
        case 'fallthrough':
          using d5 = {
            [Symbol.dispose]() {
              disposed.push(5);
            }
          };
        case 'fall': {
          let a = 0, b = () => a;
          break;
        }
      }
    } finally {
      disposed.push(6);
    }
  }
  testDisposableWithThrowAndSwitch('break');
  assertArrayEq(disposed, [2, 1, 6]);
  disposed = [];
  assertThrowsInstanceOf(() => testDisposableWithThrowAndSwitch('throw'), Error);
  assertArrayEq(disposed, [3, 1, 6]);
  disposed = [];
  assertThrowsInstanceOf(() => testDisposableWithThrowAndSwitch('throw_with_extra_scope'), Error);
  assertArrayEq(disposed, [4, 1, 6]);
  disposed = [];
  testDisposableWithThrowAndSwitch('fallthrough');
  assertArrayEq(disposed, [5, 1, 6]);
}

{
  const disposed = [];

  function testDisposeWithTryAndScopes() {
    try {
      using d = {
        [Symbol.dispose]() {
          disposed.push(1);
        }
      };
      {
        using d2 = {
          [Symbol.dispose]() {
            disposed.push(2);
          }
        };
        {
          let a = 0, b = () => a;
          {
            throw new Error("err");
          }
        }
      }
    } catch (e) {
      disposed.push(3);
    } finally {
      using d3 = {
        [Symbol.dispose]() {
          disposed.push(4);
        }
      };
      disposed.push(5);
    }
  }
  testDisposeWithTryAndScopes();
  assertArrayEq(disposed, [2, 1, 3, 5, 4]);
}

{
  const disposed = [];

  function testTryWithJumpsOutOfLoopFromTryWithBreak() {
    const w = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
    outer: for (let a of [1]) {
      for (using a of [w]) {
        try {
          using x = {
            [Symbol.dispose]() {
              disposed.push(1);
            }
          };
          break outer;
        } catch {
        } finally {
          using y = {
            [Symbol.dispose]() {
              disposed.push(2);
            }
          };
          disposed.push(3);
        }
      }
    }
  }
  testTryWithJumpsOutOfLoopFromTryWithBreak();
  assertArrayEq(disposed, [1, 3, 2, 0]);
}

{
  const disposed = [];

  function testTryWithJumpsOutOfLoopFromTryWithContinue() {
    const w = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
    outer: for (let a of [1]) {
      for (using a of [w]) {
        try {
          using x = {
            [Symbol.dispose]() {
              disposed.push(1);
            }
          };
          continue outer;
        } catch {
        } finally {
          using y = {
            [Symbol.dispose]() {
              disposed.push(2);
            }
          };
          disposed.push(3);
        }
      }
    }
  }
  testTryWithJumpsOutOfLoopFromTryWithContinue();
  assertArrayEq(disposed, [1, 3, 2, 0]);
}

{
  const disposed = [];

  function testTryWithJumpsOutOfLoopFromCatchWithBreak() {
    const w = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
    outer: for (let a of [1]) {
      for (using a of [w]) {
        try {
          using x = {
            [Symbol.dispose]() {
              disposed.push(1);
            }
          };
          throw new Error("err");
        } catch {
          using z = {
            [Symbol.dispose]() {
              disposed.push(4);
            }
          };
          break outer;
        } finally {
          using y = {
            [Symbol.dispose]() {
              disposed.push(2);
            }
          };
          disposed.push(3);
        }
      }
    }
  }
  testTryWithJumpsOutOfLoopFromCatchWithBreak();
  assertArrayEq(disposed, [1, 4, 3, 2, 0]);
}

{
  const disposed = [];

  function testTryWithJumpsOutOfLoopFromCatchWithContinue() {
    const w = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
    outer: for (let a of [1, 2]) {
      for (using a of [w]) {
        try {
          using x = {
            [Symbol.dispose]() {
              disposed.push(1);
            }
          };
          throw new Error("err");
        } catch {
          using z = {
            [Symbol.dispose]() {
              disposed.push(4);
            }
          };
          continue outer;
        }
      }
    }
  }
  testTryWithJumpsOutOfLoopFromCatchWithContinue();
  assertArrayEq(disposed, [1, 4, 0, 1, 4, 0]);
}

{
  const disposed = [];

  function testTryWithJumpsOutOfLoopFromFinallyWithBreak() {
    const w = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
    outer: for (let a of [1, 2]) {
      for (using a of [w]) {
        try {
          using x = {
            [Symbol.dispose]() {
              disposed.push(1);
            }
          };
        } finally {
          using z = {
            [Symbol.dispose]() {
              disposed.push(4);
            }
          };
          break outer;
        }
      }
    }
  }
  testTryWithJumpsOutOfLoopFromFinallyWithBreak();
  assertArrayEq(disposed, [1, 4, 0]);
}

{
  const disposed = [];

  function testTryWithJumpsOutOfLoopFromFinallyWithContinue() {
    const w = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
    outer: for (let a of [1, 2]) {
      for (using a of [w]) {
        try {
          using x = {
            [Symbol.dispose]() {
              disposed.push(1);
            }
          };
        } finally {
          using z = {
            [Symbol.dispose]() {
              disposed.push(4);
            }
          };
          continue outer;
        }
      }
    }
  }
  testTryWithJumpsOutOfLoopFromFinallyWithContinue();
  assertArrayEq(disposed, [1, 4, 0, 1, 4, 0]);
}

{
  const disposed = [];

  function testTryWithJumpsOutOfLoopExtraScope() {
    const w = {
      [Symbol.dispose]() {
        disposed.push(0);
      }
    }
    outer: for (let a of [1, 2]) {
      for (using a of [w]) {
        try {
          using x = {
            [Symbol.dispose]() {
              disposed.push(1);
            }
          };
        } finally {
          using z = {
            [Symbol.dispose]() {
              disposed.push(4);
            }
          };
          {
            let a = 0, b = () => a;
            break outer;
          }
        }
      }
    }
  }
  testTryWithJumpsOutOfLoopExtraScope();
  assertArrayEq(disposed, [1, 4, 0]);
}

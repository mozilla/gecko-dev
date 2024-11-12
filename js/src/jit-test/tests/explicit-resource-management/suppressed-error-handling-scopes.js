// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  function testExceptionBeforeDispose() {
    throw new Error("test");
    using d = {
      [Symbol.dispose]() {
        disposed.push(1);
      }
    };
  }
  assertThrowsInstanceOf(testExceptionOutsideDispose, Error);
  assertEq(disposed.length, 0);
}

{
  const disposed = [];
  function testExceptionOutsideDispose() {
    using d = {
      [Symbol.dispose]() {
        disposed.push(1);
      }
    };

    throw new Error("test");
  }
  assertThrowsInstanceOf(testExceptionOutsideDispose, Error);
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  function testExceptionInsideDispose() {
    using d = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw new Error("test");
      }
    };
  }
  assertThrowsInstanceOf(testExceptionInsideDispose, Error);
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), new Error("test2")];
  function testExceptionInsideAndOutsideDispose() {
    using d = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    };

    throw errorsToThrow[1];
  }
  assertSuppressionChain(testExceptionInsideAndOutsideDispose, errorsToThrow);
  assertArrayEq(disposed, [1]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), new Error("test2"), new Error("test3")];
  function testMultipleDisposeWithException() {
    using d1 = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    };
    using d2 = {
      [Symbol.dispose]() {
        disposed.push(2);
        throw errorsToThrow[1];
      }
    };
    using d3 = {
      [Symbol.dispose]() {
        disposed.push(3);
        throw errorsToThrow[2];
      }
    }
  }
  assertSuppressionChain(testMultipleDisposeWithException, errorsToThrow);
  assertArrayEq(disposed, [3, 2, 1]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), new Error("test2"), new Error("test3"), new Error("test4")];
  function testMultipleDisposeWithThrowsAndOutsideThrow() {
    using d1 = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    };
    using d2 = {
      [Symbol.dispose]() {
        disposed.push(2);
        throw errorsToThrow[1];
      }
    };
    using d3 = {
      [Symbol.dispose]() {
        disposed.push(3);
        throw errorsToThrow[2];
      }
    }
    throw errorsToThrow[3];
  }
  assertSuppressionChain(testMultipleDisposeWithThrowsAndOutsideThrow, errorsToThrow);
  assertArrayEq(disposed, [3, 2, 1]);
}

{
  const disposed = [];
  const errorsToThrow = [new Error("test1"), new Error("test2"), new Error("test3")];
  function testDisposeWithThrowInAnInnerScope() {
    using d1 = {
      [Symbol.dispose]() {
        disposed.push(1);
        throw errorsToThrow[0];
      }
    };
    {
      using d2 = {
        [Symbol.dispose]() {
          disposed.push(2);
          throw errorsToThrow[1];
        }
      };
      {
        let a = 0, b = () => a;
        throw errorsToThrow[2];
      }
    }
  }
  assertSuppressionChain(testDisposeWithThrowInAnInnerScope, errorsToThrow);
  assertArrayEq(disposed, [2, 1]);
}

{
  let disposed = [];
  const errorsToThrow = [new Error('test1'), new Error('test2'), new Error('test3')];
  function testDisposeWithThrowInSwitchCase(cs) {
    switch (cs) {
      case "only_1_dispose":
        using x = {
          [Symbol.dispose]() {
            disposed.push(1);
            throw errorsToThrow[0];
          }
        }
        break;
      case "2_dispose":
        using y = {
          [Symbol.dispose]() {
            disposed.push(1);
            throw errorsToThrow[0];
          }
        };
        using z = {
          [Symbol.dispose]() {
            disposed.push(2);
            throw errorsToThrow[1];
          }
        };
        break;
      case "dispose_with_outside":
        using a = {
          [Symbol.dispose]() {
            disposed.push(1);
            throw errorsToThrow[0];
          }
        };
        using b = {
          [Symbol.dispose]() {
            disposed.push(2);
            throw errorsToThrow[1];
          }
        };
        throw errorsToThrow[2];
        break;
      case 'fallthrough':
        using aa = {
          [Symbol.dispose]() {
            disposed.push(1);
            throw errorsToThrow[0];
          }
        };
      case 'fall':
        using bb = {
          [Symbol.dispose]() {
            disposed.push(2);
            throw errorsToThrow[1];
          }
        }
        throw errorsToThrow[2];
    }
  }
  assertThrowsInstanceOf(() => testDisposeWithThrowInSwitchCase('only_1_dispose'), Error);
  assertArrayEq(disposed, [1]);
  disposed = [];
  assertSuppressionChain(() => testDisposeWithThrowInSwitchCase('2_dispose'), [errorsToThrow[0], errorsToThrow[1]]);
  assertArrayEq(disposed, [2,1]);
  disposed = [];
  assertSuppressionChain(() => testDisposeWithThrowInSwitchCase('dispose_with_outside'), errorsToThrow);
  assertArrayEq(disposed, [2,1]);
  disposed = [];
  assertSuppressionChain(() => testDisposeWithThrowInSwitchCase('fallthrough'), errorsToThrow);
  assertArrayEq(disposed, [2,1]);
}

{
  globalThis.disposedModule = [];
  globalThis.errorsToThrowModule = [new Error('test1'), new Error('test2'), new Error('test3')];
  const m = parseModule(`
    using x = {
      [Symbol.dispose]() {
        globalThis.disposedModule.push(1);
        throw globalThis.errorsToThrowModule[0];
      }
    }
    using y = {
      [Symbol.dispose]() {
        globalThis.disposedModule.push(2);
        throw globalThis.errorsToThrowModule[1];
      }
    }
    throw globalThis.errorsToThrowModule[2];
  `);
  moduleLink(m);
  let e = null;
  moduleEvaluate(m).catch((err) => { e = err; });
  drainJobQueue();
  assertSuppressionChain(() => { throw e; }, globalThis.errorsToThrowModule);
  assertArrayEq(globalThis.disposedModule, [2, 1]);
}

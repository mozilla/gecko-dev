// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

class CustomError extends Error {}

{
  const disposed = [];
  let catchCalled = false;
  function testDisposalErrorIsNotCaughtByNonEnclosingTry() {
    using x = {
      [Symbol.dispose]() {
        disposed.push('a');
        throw new CustomError("dispose error");
      }
    }

    try {
      return;
    } catch {
      catchCalled = true;
    }
  }
  assertThrowsInstanceOf(testDisposalErrorIsNotCaughtByNonEnclosingTry, CustomError);
  assertArrayEq(disposed, ['a']);
  assertEq(catchCalled, false);
}

{
  const disposed = [];
  let catchCalled = false;
  function testDisposalErrorIsNotCaughtByNonEnclosingTryWhenLabelledBlocks() {
    outer: {
      using x = {
        [Symbol.dispose]() {
          disposed.push('a');
          throw new CustomError("dispose error");
        }
      }
  
      try {
        break outer;
      } catch {
        catchCalled = true;
      }
    }
  }

  assertThrowsInstanceOf(testDisposalErrorIsNotCaughtByNonEnclosingTryWhenLabelledBlocks, CustomError);
  assertArrayEq(disposed, ['a']);
  assertEq(catchCalled, false);
}

{
  const disposed = [];
  let catchCalled = false;
  function testDisposalErrorIsNotCaughtByNonEnclosingTryWhenSwitchCase() {
    switch (1) {
      case 1:
        using x = {
          [Symbol.dispose]() {
            disposed.push('a');
            throw new CustomError("dispose error");
          }
        }
        try {
          break;
        } catch {
          catchCalled = true;
        }
    }
  }

  assertThrowsInstanceOf(testDisposalErrorIsNotCaughtByNonEnclosingTryWhenSwitchCase, CustomError);
  assertArrayEq(disposed, ['a']);
  assertEq(catchCalled, false);
}

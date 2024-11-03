// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const disposed = [];
  let wasStartedBeforeAwait = false;
  let didEvaluatePrecedingBlockStatementsBeforeAwait = false;
  let didEvaluateFollowingBlockStatementsBeforeAwait = false;
  let allStatementsRanBeforeAwait = false;

  async function testAwaitUsingCausesAnAwait(evaluateAwaitUsing) {
    let isRunningBeforeAwait = true;

    async function f() {
      wasStartedBeforeAwait = isRunningBeforeAwait;
      outer: {
        didEvaluatePrecedingBlockStatementsBeforeAwait = isRunningBeforeAwait;
        if (!evaluateAwaitUsing) break outer;
        await using _ = {
          [Symbol.asyncDispose]() {
            disposed.push(1);
          }
        };
        didEvaluateFollowingBlockStatementsBeforeAwait = isRunningBeforeAwait;
      }
      allStatementsRanBeforeAwait = isRunningBeforeAwait;
    }

    let p = f();
    isRunningBeforeAwait = false;
    await p;
  }
  testAwaitUsingCausesAnAwait(false);
  drainJobQueue();
  assertEq(wasStartedBeforeAwait, true);
  assertEq(didEvaluatePrecedingBlockStatementsBeforeAwait, true);
  assertEq(allStatementsRanBeforeAwait, true);
  assertArrayEq(disposed, []);

  wasStartedBeforeAwait = false;
  didEvaluatePrecedingBlockStatementsBeforeAwait = false;
  didEvaluateFollowingBlockStatementsBeforeAwait = false;
  allStatementsRanBeforeAwait = false;
  testAwaitUsingCausesAnAwait(true);
  drainJobQueue();
  assertEq(wasStartedBeforeAwait, true);
  // the await using statement is supposed to cause an await right before the scope
  // end thus the block statements should be evaluated synchronously until the scope end.
  assertEq(didEvaluatePrecedingBlockStatementsBeforeAwait, true);
  assertEq(didEvaluateFollowingBlockStatementsBeforeAwait, true);
  assertEq(allStatementsRanBeforeAwait, false);
  assertArrayEq(disposed, [1]);
}

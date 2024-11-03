// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

{
  let called = false;
  async function testSyncThrowIsRejection() {
    await using x = {
      [Symbol.dispose]() {
        throw 1;
      }
    }
  }
  testSyncThrowIsRejection().catch(e => {
    called = true;
    assertEq(e, 1);
  });
  drainJobQueue();
  assertEq(called, true);
}

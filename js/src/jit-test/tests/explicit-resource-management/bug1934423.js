// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

gczeal(8,18);
{
  const values = [];
  async function* gen() {
    await using a65 = {
      [Symbol.asyncDispose]() {}
    }
    await using b61 = {

    }
  }
  async function testDisposalDuringForcedThrowInGenerator() {
    let x91 = gen();
    values.push((await x91.next()).value);
    try {} catch (e6) {}
  }
  for (let i = 0; i < 20; i++) {
    testDisposalDuringForcedThrowInGenerator().catch(() => {});
  }
}

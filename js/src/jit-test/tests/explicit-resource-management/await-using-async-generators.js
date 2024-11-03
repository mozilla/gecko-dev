// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

{
  const values = [];
  let res;
  const suspend = new Promise((resolve, reject) => {
    res = resolve;
  });
  async function* gen() {
    yield 1;
    yield await suspend;
    yield 3;
    await using d = {
      [Symbol.asyncDispose]() {
        values.push(6);
      }
    }
    await using d2 = {
      [Symbol.asyncDispose]() {
        values.push(5);
      }
    }
    yield 4;
  }
  async function testDisposalInAsyncGenerator() {
    const g = gen();
    values.push((await g.next()).value);
    res(2);
    values.push((await g.next()).value);
    values.push((await g.next()).value);
    values.push((await g.next()).value);
    await g.next();
  }
  testDisposalInAsyncGenerator();
  drainJobQueue();
  assertArrayEq(values, [1, 2, 3, 4, 5, 6]);
}

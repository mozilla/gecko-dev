// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management"); --enable-explicit-resource-management

load(libdir + "asserts.js");

const values = [];

async function* gen() {
  using d = {
    value: "d",
    [Symbol.dispose]() {
      values.push(this.value);
    }
  }
  yield await Promise.resolve("a");
  yield await Promise.resolve("b");
  using c = {
    value: "c",
    [Symbol.dispose]() {
      values.push(this.value);
    }
  }
  throw new Error("err");
}

async function testThrowInAsyncGenerator() {
  let x = gen();
  values.push((await x.next()).value);
  values.push((await x.next()).value);
  await x.next().catch(() => {});
}
testThrowInAsyncGenerator();
drainJobQueue();
assertArrayEq(values, ["a", "b", "c", "d"]);

const values2 = [];
async function* gen2() {
  using c = {
    value: "c",
    [Symbol.dispose]() {
      values2.push(this.value);
    }
  }
  yield await Promise.resolve("a");
  yield await Promise.resolve("b");
  return;
}

async function testForcedThrowInAsyncGenerator() {
  let x = gen2();
  values2.push((await x.next()).value);
  await x.throw(new Error("err")).catch(() => {});
}
testForcedThrowInAsyncGenerator();
drainJobQueue();
assertArrayEq(values2, ["a", "c"]);

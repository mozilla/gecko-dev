function testOwnConstructorProp() {
  let p = Promise.resolve(0);
  let c = 0;
  Object.defineProperty(p, "constructor", {get() {
    c++;
    return Promise;
  }});
  p.then(() => {});
  p.then(() => {});
  assertEq(c, 2);
}
testOwnConstructorProp();

function testOtherProtoConstructorProp() {
  let p = Promise.resolve(0);
  let proto = Object.create(Promise.prototype);
  Object.setPrototypeOf(p, proto);
  let c = 0;
  Object.defineProperty(proto, "constructor", {get() {
    c++;
    return Promise;
  }});
  p.then(() => {});
  p.then(() => {});
  assertEq(c, 2);
}
testOtherProtoConstructorProp();

function testOwnThenProp() {
  let p = new Promise(function() {});
  let c = 0;
  Object.defineProperty(p, "then", {get() {
    c++;
    return Promise.prototype.then;
  }});
  p.catch();
  assertEq(c, 1);
}
testOwnThenProp();

function testOtherProtoThenProp() {
  let p = new Promise(function() {});
  let proto = Object.create(Promise.prototype);
  Object.setPrototypeOf(p, proto);
  let c = 0;
  Object.defineProperty(proto, "then", {get() {
    c++;
    return Promise.prototype.then;
  }});
  p.catch();
  assertEq(c, 1);
}
testOtherProtoThenProp();

// Fuse should still be intact after all that.
assertEq(getFuseState().OptimizePromiseLookupFuse.intact, true);

function testClobberResolve() {
  newGlobal().evaluate(`
    let origResolve = Promise.resolve;
    let c = 0;
    Promise.resolve = function() {
      c++;
      return origResolve.apply(this, arguments);
    };
    let p1 = new Promise(function() {});
    let p2 = new Promise(function() {});
    Promise.all([p1, p2]);
    assertEq(c, 2);
  `);
}
testClobberResolve();

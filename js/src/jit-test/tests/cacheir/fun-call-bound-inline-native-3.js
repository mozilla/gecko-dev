// Test inlining bound natives through Function.prototype.call
//
// Array() is inlined when there are 0-1 arguments.

function arrayThisAbsent() {
  var Arr = Array.bind();
  for (let i = 0; i < 400; ++i) {
    let r = Arr.call();
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) arrayThisAbsent();

function array0() {
  var Arr = Array.bind();
  for (let i = 0; i < 400; ++i) {
    let r = Arr.call(null);
    assertEq(r.length, 0);
  }
}
for (let i = 0; i < 2; ++i) array0();

function array0bound1() {
  var Arr = Array.bind(null, 3);
  for (let i = 0; i < 400; ++i) {
    let r = Arr.call(null);
    assertEq(r.length, 3);
  }
}
for (let i = 0; i < 2; ++i) array0bound1();

function array1() {
  var Arr = Array.bind();
  for (let i = 0; i < 400; ++i) {
    let r = Arr.call(null, 5);
    assertEq(r.length, 5);
  }
}
for (let i = 0; i < 2; ++i) array1();

function array1bound1() {
  var Arr = Array.bind(null, 3);
  for (let i = 0; i < 400; ++i) {
    let r = Arr.call(null, 5);
    assertEq(r.length, 2);
  }
}
for (let i = 0; i < 2; ++i) array1bound1();

function array2() {
  var Arr = Array.bind();
  for (let i = 0; i < 400; ++i) {
    let r = Arr.call(null, 5, 10);
    assertEq(r.length, 2);
  }
}
for (let i = 0; i < 2; ++i) array2();

function array2bound4() {
  var Arr = Array.bind(null, 3, 4, 5, 6);
  for (let i = 0; i < 400; ++i) {
    let r = Arr.call(null, 5, 10);
    assertEq(r.length, 6);
  }
}
for (let i = 0; i < 2; ++i) array2bound4();

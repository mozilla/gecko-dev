function main() {
  // A comment so we can test that breakpoint sliding works across
  // multiple lines
  const func = foo(1, 2);
  const result = func();
  return result;
}

function doEval() {
  eval(
    "(" +
      function() {
        debugger;

        window.evaledFunc = function() {
          var foo = 1;
          var bar = 2;
          return foo + bar;
        };
      }.toString() +
      ")()"
  );
}

function doNamedEval() {
  eval(
    "(" +
      function() {
        debugger;

        window.evaledFunc = function() {
          var foo = 1;
          var bar = 2;
          return foo + bar;
        };
      }.toString() +
      ")();\n //# sourceURL=evaled.js"
  );
}

class MyClass {
  constructor(a, b, ...rest) {
    this.#a = a;
    this.#b = b;
  }

  #a;
  #b;

  test( ...args) {}
  #privateFunc(a, b) { }
}

class Klass {
  constructor() {
    this.id = Math.random();
  }
  test() { }
  bar = function () { }
  boo = (a, ...b) => {}
}

const o = 1 + 2;
console.log(o);

function normalFunction(foo, ...bar) { }
let letFunction = function (a) { };
const constFunction = (x) => {};

function ProtoClass(a) {};
ProtoClass.prototype = {
  protoFoo(foo, ...bar) { },
  protoBar: function (x, y) { },
  protoBoo: (x) => { },
  1234: () => { },
};

const bla = {}
bla.memFoo = function(a, b) { }
bla.arrFoo = (c) => { }

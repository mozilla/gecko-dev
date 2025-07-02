var g = newGlobal({ newCompartment: true });
g.eval(`
let counter = 10;

function func1() {
  var local1 = counter;

  var func2 = function f2() {
    var local2 = counter;

    var func3 = function f3() {
      var local3 = counter;

      var func4 = function f4() {
        var local4 = counter;

        // Close these local variables over to make them available in
        // the environment objects.
        var x = [local1, local2, local3];

        counter++;

        debugger;
      };

      func4();
    };

    func3();
  };

  func2();
};
`);

var dbg = new Debugger(g);

var result = null;

dbg.onDebuggerStatement = function handleDebuggerStatement(f) {
  result = {
    local1: f.eval("local1").return,
    local2: f.eval("local2").return,
    local3: f.eval("local3").return,
    local4: f.eval("local4").return,
  };
};

g.eval('func1()');

// All references, with/without the function call bounrary, should see
// the same value.
assertEq(result.local1, 10);
assertEq(result.local2, 10);
assertEq(result.local3, 10);
assertEq(result.local4, 10);

g.eval('func1()');

// All references, with/without the function call bounrary, should see
// the same updated value in the 2nd call's environment chain.
assertEq(result.local1, 11);
assertEq(result.local2, 11);
assertEq(result.local3, 11);
assertEq(result.local4, 11);

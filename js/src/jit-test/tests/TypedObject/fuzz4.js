// |jit-test| error:Error;

var A = TypedObject.uint8.array(10);
var a = new A();
a.forEach(function(val, i) {
  assertEq(arguments[5], a);
});

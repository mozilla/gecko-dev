// |jit-test| error: InternalError: too much recursion
function f() {
  moduleLink(parseModule("[]", "", "json"));
  Math.valueOf = f;
  Math.pow(Math);
}
f();


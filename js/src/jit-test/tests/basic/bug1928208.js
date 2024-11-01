// |jit-test| --dump-bytecode
gczeal(22, 150);
a = [""];
"".toString();
Object.defineProperty(a, 5, {get : function() {
  gc(this, "shrinking");
}})

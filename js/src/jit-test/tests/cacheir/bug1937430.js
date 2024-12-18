// |jit-test| skip-if: !hasDisassembler()

gczeal(19);
for (let i = 0; i < 99; i++) {
  function g() {
    function f() {
      with ({}) {}
    }
    class C extends f {}
  }
  g();
  disblic(g);
}

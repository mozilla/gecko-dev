// |jit-test| --fuzzing-safe
gczeal(0);
gczeal(8, 37);
function f67() {
  for (let i71 = 0; i71 < 5; i71++) {
    print(i71);
    const v82 = this.transplantableObject();
    const v83 = v82.object;
    class C84 {};
    const o86 = {
      "sameZoneAs": C84,
      "immutablePrototype": false,
    };
    const t60 = newGlobal(o86);
    t60.__proto__ = v83;
    const v90 = newGlobal({sameCompartmentAs: this});
    v90.nukeAllCCWs();
    v82.transplant(v90);
  }
}
f67();

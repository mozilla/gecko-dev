if (!this.enqueueMark) {
  quit();
}

let global = newGlobal({newCompartment: true});
let original = new FakeDOMObject();
original['bar'] = {};

let {object, transplant} = transplantableObject({object: original});
assertEq(object, original)

enqueueMark(object);
enqueueMark("yield");

gczeal(0);
gczeal(8);
startgc(1);
gcslice(1);
transplant(global);

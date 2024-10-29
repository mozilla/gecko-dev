var obj;
const proto = {
  get prop() {
    Object.defineProperty(obj, "prop", { value:  true});
    return false;
  }
}
obj = new Object(proto);
obj[Symbol.unscopables] = proto;
with (obj) {
  assertEq(prop, true);
}

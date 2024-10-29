var obj;
const proto = {
  get prop() {
    delete proto.prop;
    return false;
  }
}
obj = new Object(proto);
obj[Symbol.unscopables] = proto;
with (obj) {
  assertEq(prop, undefined);
}

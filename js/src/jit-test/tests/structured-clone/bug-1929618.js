// |jit-test| error:Error

const v1 = serialize();
var queue = [[deserialize(v1),v1]];
const v8 = queue.shift()[1];
function f9(a10) {
    return Object.getOwnPropertyDescriptor(v8, a10).get.apply(v1, Object);
}
Object.getOwnPropertyNames(v8).map(f9);

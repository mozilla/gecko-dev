assertEq(getUseCounterResults().ThenableUse, 0);
assertEq(getUseCounterResults().ThenableUseProto, 0);
assertEq(getUseCounterResults().ThenableUseStandardProto, 0);

function test_thenable(obj, keys) {
    let before = keys.map(prop => getUseCounterResults()[prop]);
    Promise.resolve(obj);
    let after = keys.map(prop => getUseCounterResults()[prop]);
    for (var i = 0; i < keys.length; i++) {
        assertEq(before[i] + 1, after[i]);
    }
}

let obj = { then: () => { console.log("hello"); } };
test_thenable(obj, ["ThenableUse"]);

let obj_with_proto = { __proto__: obj };
test_thenable(obj_with_proto, ["ThenableUse", "ThenableUseProto"]);

Array.prototype.then = () => { console.log("hello"); };
test_thenable([], ["ThenableUse", "ThenableUseProto", "ThenableUseStandardProto"]);

Object.prototype.then = () => { console.log("then"); }
test_thenable({}, ["ThenableUse", "ThenableUseProto", "ThenableUseStandardProto"]);

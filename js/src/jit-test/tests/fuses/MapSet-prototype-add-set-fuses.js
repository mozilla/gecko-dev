// Mutating Map.prototype.set pops the fuse. A no-op change is fine.
assertEq(getFuseState().OptimizeMapPrototypeSetFuse.intact, true);
let v1 = Map.prototype.set;
Map.prototype.set = v1;
assertEq(getFuseState().OptimizeMapPrototypeSetFuse.intact, true);
Map.prototype.set = function() {};
assertEq(getFuseState().OptimizeMapPrototypeSetFuse.intact, false);

// Same for Set.prototype.add.
assertEq(getFuseState().OptimizeSetPrototypeAddFuse.intact, true);
let v2 = Set.prototype.add;
Set.prototype.add = v2;
assertEq(getFuseState().OptimizeSetPrototypeAddFuse.intact, true);
delete Set.prototype.add;
assertEq(getFuseState().OptimizeSetPrototypeAddFuse.intact, false);

// Same for WeakMap.prototype.set.
assertEq(getFuseState().OptimizeWeakMapPrototypeSetFuse.intact, true);
let v3 = WeakMap.prototype.set;
WeakMap.prototype.set = v3;
assertEq(getFuseState().OptimizeWeakMapPrototypeSetFuse.intact, true);
WeakMap.prototype.set = function() {};
assertEq(getFuseState().OptimizeWeakMapPrototypeSetFuse.intact, false);

// Same for WeakSet.prototype.add.
assertEq(getFuseState().OptimizeWeakSetPrototypeAddFuse.intact, true);
let v4 = WeakSet.prototype.add;
WeakSet.prototype.add = v4;
assertEq(getFuseState().OptimizeWeakSetPrototypeAddFuse.intact, true);
delete WeakSet.prototype.add;
assertEq(getFuseState().OptimizeWeakSetPrototypeAddFuse.intact, false);

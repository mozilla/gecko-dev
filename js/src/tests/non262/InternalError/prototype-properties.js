assertEq(Reflect.ownKeys(InternalError.prototype).toString(), "message,name,constructor");
assertEq(InternalError.prototype.name, InternalError.name);
assertEq(InternalError.prototype.message, "");
assertEq(InternalError.prototype.constructor, InternalError);

if (typeof reportCompare === "function")
    reportCompare(0, 0);

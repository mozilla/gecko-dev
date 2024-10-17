// |jit-test| error:Error
var x = String.fromCharCode(929);
parseModule(x, x, x);

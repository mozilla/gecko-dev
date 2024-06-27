// |jit-test| error: Error
var mod = parseModule("a");
d = newGlobal().registerModule("c", mod);
moduleLink(d);
moduleEvaluate(d);

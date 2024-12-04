var code = cacheEntry("assertEq('bar', String.raw`bar`);");
var g = newGlobal({ cloneSingletons: true });
evaluate(code, { global: g, saveBytecodeWithDelazifications: true });
evaluate(code, { global: g, loadBytecode: true })

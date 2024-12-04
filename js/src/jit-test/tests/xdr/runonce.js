load(libdir + "asserts.js")

// Incremental XDR doesn't have run-once script restrictions.
evaluate(cacheEntry(""), { saveBytecodeWithDelazifications: true });
evaluate(cacheEntry(""), { saveBytecodeWithDelazifications: true, isRunOnce: false });
evaluate(cacheEntry(""), { saveBytecodeWithDelazifications: true, isRunOnce: true });

load(libdir + "asserts.js");

var entry = streamCacheEntry(new ArrayBuffer());
assertErrorMessage(() => entry.getBuffer.call(),
                   Error, /Expected StreamCacheEntry/);
assertErrorMessage(() => Object.getOwnPropertyDescriptor(entry, "cached").get(),
                   Error, /Expected StreamCacheEntry/);

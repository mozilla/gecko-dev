// |jit-test| --enable-uint8array-base64
var arr = new Uint8Array();
oomTest(() => arr.toBase64(0));

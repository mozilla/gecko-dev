let arr = new ArrayBuffer(4, { maxByteLength: 4 })

new Int8Array(arr);
grayRoot()[0]  = new Int8Array(arr)

gc();
detachArrayBuffer(arr);

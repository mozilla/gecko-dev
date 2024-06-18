// Invalid data should not be able to cause a stack overflow in JSStructuredCloneReader.

// This test works treats the underlying data format as a black box. It starts
// with valid serialized data and mutates it by repeating a slice thousands of
// times. The engine should reject the result as invalid and not crash.

const REPEAT_SIZE_BYTES = 16;  // size of repeating slice
const NREPEATS = 50000;        // number of times to repeat it
const STEP_SIZE_BYTES = 8;     // how far apart we should try cutting

// First, get a typed array containing good serialized data,
// encoded to be sent across a process boundary.
let originalObject = new Uint16Array(new ArrayBuffer(8));
let goodSerializedData = serialize(originalObject, [], { scope: "DifferentProcess" });
let goodBytes = new Uint8Array(goodSerializedData.arraybuffer);
assertEq(goodBytes.length % 8, 0, "this test expects serialized data to consist of 64-bit units");

for (let i = 0; i + REPEAT_SIZE_BYTES <= goodBytes.length; i += STEP_SIZE_BYTES) {
    // The first i words of badBytes are identical to goodBytes.
    let badBytes = new Uint8Array(i + NREPEATS * REPEAT_SIZE_BYTES);
    badBytes.set(goodBytes.slice(0, i), 0);

    // The rest consists of a slice of goodBytes repeated over and over.
    let slab = goodBytes.slice(i, i + REPEAT_SIZE_BYTES);
    for (let j = i; j < badBytes.length; j += REPEAT_SIZE_BYTES)
        badBytes.set(slab, j);
    // print(uneval(Array.from(badBytes.slice(0, i + 2 * REPEAT_SIZE_BYTES))));

    // Construct a bad serialized-data object from the array.
    let badSerializedData = serialize({}, [], { scope: "DifferentProcess" });
    badSerializedData.arraybuffer = badBytes.buffer;

    // Now try deserializing it.
    try {
        deserialize(badSerializedData);
        assertEq(false, true, "no error");
    } catch (exc) {
        assertEq(true, exc instanceof InternalError || exc instanceof RangeError);
    }
}

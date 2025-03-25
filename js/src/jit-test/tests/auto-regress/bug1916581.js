function blackhole() {
  with ({});
}

// Atomics.load emits:
//   arraybufferviewelements = MArrayBufferViewElements(typedarray)
//   int64 = MLoadUnboxedScalar(arraybufferviewelements, index)
//   bigint = MInt64ToBigInt(int64)
//   <resume-after MInt64ToBigInt>
//
// TypedArray access with out-of-bounds supports emits:
//   arraybufferviewelements = MArrayBufferViewElements(typedarray)
//   value = MLoadTypedArrayElementHole(arraybufferviewelements, index)
//
// Both instructions use MArrayBufferViewElements, so instruction reordering
// may move MLoadTypedArrayElementHole to shorten the life time of
// MArrayBufferViewElements. But instruction reordering must not reorder
// MInt64ToBigInt to happen after MLoadTypedArrayElementHole, because
// MLoadTypedArrayElementHole uses a safe point and we require that all
// instruction captured by a resume point are lowered before encoding the safe
// point.
//
// BAD:
//   arraybufferviewelements = MArrayBufferViewElements(typedarray)
//   int64 = MLoadUnboxedScalar(arraybufferviewelements, index)
//   value = MLoadTypedArrayElementHole(arraybufferviewelements, index)
//   bigint = MInt64ToBigInt(int64)
//
// GOOD:
//   arraybufferviewelements = MArrayBufferViewElements(typedarray)
//   int64 = MLoadUnboxedScalar(arraybufferviewelements, index)
//   bigint = MInt64ToBigInt(int64)
//   value = MLoadTypedArrayElementHole(arraybufferviewelements, index)

function f1() {
  const i64 = new BigInt64Array(1);

  for (let i = 0; i < 100; i++) {
    // Atomics.load has a resume point and MInt64ToBigInt.
    let x = Atomics.load(i64, 0);

    // MLoadTypedArrayElementHole with always out-of-bounds index.
    // MLoadTypedArrayElementHole has a safe point.
    let y = i64[2];

    blackhole(x, y);
  }
}
f1();

function f2() {
  const i64 = new BigInt64Array(1);

  for (let i = 0; i < 100; i++) {
    let j = i & 3;

    // Add another use for |j|, so |y| doesn't add an MInt32ToIntPtr node
    // which can prevent instruction reordering.
    let z = i64[j];

    // Atomics.load has a resume point and MInt64ToBigInt.
    let x = Atomics.load(i64, 0);

    // MLoadTypedArrayElementHole with maybe out-of-bounds index.
    // MLoadTypedArrayElementHole has a safe point.
    let y = i64[j];

    blackhole(x, y, z);
  }
}
f2();

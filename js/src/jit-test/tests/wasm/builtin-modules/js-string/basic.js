// |jit-test| skip-if: !wasmJSStringBuiltinsEnabled();

let testModule = `(module
  (type $arrayMutI16 (array (mut i16)))

  (func $testImp
    (import "wasm:js-string" "test")
    (param externref)
    (result i32)
  )
  (func $castImp
    (import "wasm:js-string" "cast")
    (param externref)
    (result (ref extern))
  )
  (func $fromCharCodeArrayImp
    (import "wasm:js-string" "fromCharCodeArray")
    (param (ref null $arrayMutI16) i32 i32)
    (result (ref extern))
  )
  (func $intoCharCodeArrayImp
    (import "wasm:js-string" "intoCharCodeArray")
    (param externref (ref null $arrayMutI16) i32)
    (result i32)
  )
  (func $fromCharCodeImp
    (import "wasm:js-string" "fromCharCode")
    (param i32)
    (result (ref extern))
  )
  (func $fromCodePointImp
    (import "wasm:js-string" "fromCodePoint")
    (param i32)
    (result (ref extern))
  )
  (func $charCodeAtImp
    (import "wasm:js-string" "charCodeAt")
    (param externref i32)
    (result i32)
  )
  (func $codePointAtImp
    (import "wasm:js-string" "codePointAt")
    (param externref i32)
    (result i32)
  )
  (func $lengthImp
    (import "wasm:js-string" "length")
    (param externref)
    (result i32)
  )
  (func $concatImp
    (import "wasm:js-string" "concat")
    (param externref externref)
    (result (ref extern))
  )
  (func $substringImp
    (import "wasm:js-string" "substring")
    (param externref i32 i32)
    (result (ref extern))
  )
  (func $equalsImp
    (import "wasm:js-string" "equals")
    (param externref externref)
    (result i32)
  )
  (func $compareImp
    (import "wasm:js-string" "compare")
    (param externref externref)
    (result i32)
  )

  (func $test (export "test")
    (param externref)
    (result i32)
    local.get 0
    call $testImp
  )
  (func $cast (export "cast")
    (param externref)
    (result (ref extern))
    local.get 0
    call $castImp
  )
  (func $fromCharCodeArray (export "fromCharCodeArray")
    (param (ref null $arrayMutI16) i32 i32)
    (result (ref extern))
    local.get 0
    local.get 1
    local.get 2
    call $fromCharCodeArrayImp
  )
  (func $intoCharCodeArray (export "intoCharCodeArray")
    (param externref (ref null $arrayMutI16) i32)
    (result i32)
    local.get 0
    local.get 1
    local.get 2
    call $intoCharCodeArrayImp
  )
  (func $fromCharCode (export "fromCharCode")
    (param i32)
    (result externref)
    local.get 0
    call $fromCharCodeImp
  )
  (func $fromCodePoint (export "fromCodePoint")
    (param i32)
    (result externref)
    local.get 0
    call $fromCodePointImp
  )
  (func $charCodeAt (export "charCodeAt")
    (param externref i32)
    (result i32)
    local.get 0
    local.get 1
    call $charCodeAtImp
  )
  (func $codePointAt (export "codePointAt")
    (param externref i32)
    (result i32)
    local.get 0
    local.get 1
    call $codePointAtImp
  )
  (func $length (export "length")
    (param externref)
    (result i32)
    local.get 0
    call $lengthImp
  )
  (func $concat (export "concat")
    (param externref externref)
    (result externref)
    local.get 0
    local.get 1
    call $concatImp
  )
  (func $substring (export "substring")
    (param externref i32 i32)
    (result externref)
    local.get 0
    local.get 1
    local.get 2
    call $substringImp
  )
  (func $equals (export "equals")
    (param externref externref)
    (result i32)
    local.get 0
    local.get 1
    call $equalsImp
  )
  (func $compare (export "compare")
    (param externref externref)
    (result i32)
    local.get 0
    local.get 1
    call $compareImp
  )
)`;

let {
  createArrayMutI16,
  arrayLength,
  arraySet,
  arrayGet
} = wasmEvalText(`(module
  (type $arrayMutI16 (array (mut i16)))
  (func (export "createArrayMutI16") (param i32) (result anyref)
    i32.const 0
    local.get 0
    array.new $arrayMutI16
  )
  (func (export "arrayLength") (param arrayref) (result i32)
    local.get 0
    array.len
  )
  (func (export "arraySet") (param (ref $arrayMutI16) i32 i32)
    local.get 0
    local.get 1
    local.get 2
    array.set $arrayMutI16
  )
  (func (export "arrayGet") (param (ref $arrayMutI16) i32) (result i32)
    local.get 0
    local.get 1
    array.get_u $arrayMutI16
  )
)`).exports;

function throwIfNotString(a) {
  if (typeof a !== "string") {
    throw new WebAssembly.RuntimeError();
  }
}
function throwIfNotStringOrNull(a) {
  if (a !== null && typeof a !== "string") {
    throw new WebAssembly.RuntimeError();
  }
}
let polyFillImports = {
  test: (string) => {
    if (string === null ||
        typeof string !== "string") {
      return 0;
    }
    return 1;
  },
  cast: (string) => {
    if (string === null ||
        typeof string !== "string") {
      throw new WebAssembly.RuntimeError();
    }
    return string;
  },
  fromCharCodeArray: (array, arrayStart, arrayEnd) => {
    arrayStart >>>= 0;
    arrayEnd >>>= 0;
    if (array == null ||
        arrayStart > arrayEnd ||
        arrayEnd > arrayLength(array)) {
      throw new WebAssembly.RuntimeError();
    }
    let result = '';
    for (let i = arrayStart; i < arrayEnd; i++) {
      result += String.fromCharCode(arrayGet(array, i));
    }
    return result;
  },
  intoCharCodeArray: (string, arr, arrayStart) => {
    arrayStart >>>= 0;
    throwIfNotString(string);
    if (arr == null) {
      throw new WebAssembly.RuntimeError();
    }
    let arrLength = arrayLength(arr);
    let stringLength = string.length;
    if (BigInt(arrayStart) + BigInt(stringLength) > BigInt(arrLength)) {
      throw new WebAssembly.RuntimeError();
    }
    for (let i = 0; i < stringLength; i++) {
      arraySet(arr, arrayStart + i, string[i].charCodeAt(0));
    }
    return stringLength;
  },
  fromCharCode: (charCode) => {
    charCode >>>= 0;
    return String.fromCharCode(charCode);
  },
  fromCodePoint: (codePoint) => {
    codePoint >>>= 0;
    return String.fromCodePoint(codePoint);
  },
  charCodeAt: (string, stringIndex) => {
    stringIndex >>>= 0;
    throwIfNotString(string);
    if (stringIndex >= string.length)
      throw new WebAssembly.RuntimeError();
    return string.charCodeAt(stringIndex);
  },
  codePointAt: (string, stringIndex) => {
    stringIndex >>>= 0;
    throwIfNotString(string);
    if (stringIndex >= string.length)
      throw new WebAssembly.RuntimeError();
    return string.codePointAt(stringIndex);
  },
  length: (string) => {
    throwIfNotString(string);
    return string.length;
  },
  concat: (stringA, stringB) => {
    throwIfNotString(stringA);
    throwIfNotString(stringB);
    return stringA + stringB;
  },
  substring: (string, startIndex, endIndex) => {
    startIndex >>>= 0;
    endIndex >>>= 0;
    throwIfNotString(string);
    if (startIndex > string.length ||
        endIndex < startIndex) {
      return "";
    }
    if (endIndex > string.length) {
      endIndex = string.length;
    }
    return string.substring(startIndex, endIndex);
  },
  equals: (stringA, stringB) => {
    throwIfNotStringOrNull(stringA);
    throwIfNotStringOrNull(stringB);
    return stringA === stringB;
  },
  compare: (stringA, stringB) => {
    throwIfNotString(stringA);
    throwIfNotString(stringB);
    if (stringA < stringB) {
      return -1;
    }
    return stringA === stringB ? 0 : 1;
  },
};

function assertSameBehavior(funcA, funcB, ...params) {
  let resultA;
  let errA = null;
  try {
    resultA = funcA(...params);
  } catch (err) {
    errA = err;
  }

  let resultB;
  let errB = null;
  try {
    resultB = funcB(...params);
  } catch (err) {
    errB = err;
  }

  if (errA || errB) {
    assertEq(errA === null, errB === null, errA ? errA.message : errB.message);
    assertEq(Object.getPrototypeOf(errA), Object.getPrototypeOf(errB));
  }
  assertEq(resultA, resultB);

  if (errA) {
    throw errA;
  }
  return resultA;
}

let builtinExports = wasmEvalText(testModule, {}, {builtins: ["js-string"]}).exports;
let polyfillExports = wasmEvalText(testModule, { 'wasm:js-string': polyFillImports }).exports;

let testStrings = ["", "a", "1", "ab", "hello, world", "\n", "☺", "☺smiley", String.fromCodePoint(0x10000, 0x10001)];
let testStringsAndNull = [...testStrings, null];
let testCharCodes = [1, 2, 3, 10, 0x7f, 0xff, 0xfffe, 0xffff];
let testCodePoints = [1, 2, 3, 10, 0x7f, 0xff, 0xfffe, 0xffff, 0x10000, 0x10001];

for (let a of WasmExternrefValues) {
  assertSameBehavior(
    builtinExports['test'],
    polyfillExports['test'],
    a
  );
  try {
    assertSameBehavior(
      builtinExports['cast'],
      polyfillExports['cast'],
      a
    );
  } catch (err) {
    assertEq(err instanceof WebAssembly.RuntimeError, true);
  }
}

for (let a of testCharCodes) {
  assertSameBehavior(
    builtinExports['fromCharCode'],
    polyfillExports['fromCharCode'],
    a
  );
}

for (let a of testCodePoints) {
  assertSameBehavior(
    builtinExports['fromCodePoint'],
    polyfillExports['fromCodePoint'],
    a
  );
}

for (let a of testStrings) {
  let length = assertSameBehavior(
    builtinExports['length'],
    polyfillExports['length'],
    a
  );

  for (let i = 0; i < length; i++) {
    let charCode = assertSameBehavior(
      builtinExports['charCodeAt'],
      polyfillExports['charCodeAt'],
      a, i
    );
  }
  for (let i = 0; i < length; i++) {
    let charCode = assertSameBehavior(
      builtinExports['codePointAt'],
      polyfillExports['codePointAt'],
      a, i
    );
  }

  let arrayMutI16 = createArrayMutI16(length);
  assertSameBehavior(
    builtinExports['intoCharCodeArray'],
    polyfillExports['intoCharCodeArray'],
    a, arrayMutI16, 0
  );
  assertSameBehavior(
    builtinExports['fromCharCodeArray'],
    polyfillExports['fromCharCodeArray'],
    arrayMutI16, 0, length
  );

  for (let i = 0; i < length; i++) {
    // The end parameter is interpreted as unsigned and is always clamped to
    // the string length. This means that -1, and string.length + 1 are valid
    // end indices.
    for (let j = -1; j <= length + 1; j++) {
      assertSameBehavior(
        builtinExports['substring'],
        polyfillExports['substring'],
        a, i, j
      );
    }
  }
}

for (let a of testStrings) {
  for (let b of testStrings) {
    assertSameBehavior(
      builtinExports['concat'],
      polyfillExports['concat'],
      a, b
    );
    assertSameBehavior(
      builtinExports['compare'],
      polyfillExports['compare'],
      a, b
    );
  }
}

for (let a of testStringsAndNull) {
  for (let b of testStringsAndNull) {
    assertSameBehavior(
      builtinExports['equals'],
      polyfillExports['equals'],
      a, b
    );
  }
}

// fromCharCodeArray endIndex is an unsigned integer
{
  let arrayMutI16 = createArrayMutI16(1);
  assertErrorMessage(() => assertSameBehavior(
    builtinExports['fromCharCodeArray'],
    polyfillExports['fromCharCodeArray'],
    arrayMutI16, 1, -1
  ), WebAssembly.RuntimeError, /./);
}

// fromCharCodeArray is startIndex and endIndex, not a count
{
  let arrayMutI16 = createArrayMutI16(1);
  // Ask for [1, 1) to get an empty string. If misinterpreted as a count, this
  // will result in a trap.
  assertEq(assertSameBehavior(
    builtinExports['fromCharCodeArray'],
    polyfillExports['fromCharCodeArray'],
    arrayMutI16, 1, 1
  ), "");
}

// fromCharCodeArray array is null
{
  assertErrorMessage(() => assertSameBehavior(
    builtinExports['fromCharCodeArray'],
    polyfillExports['fromCharCodeArray'],
    null, 0, 0
  ), WebAssembly.RuntimeError, /./);
}

// intoCharCodeArray array is null
{
  assertErrorMessage(() => assertSameBehavior(
    builtinExports['intoCharCodeArray'],
    polyfillExports['intoCharCodeArray'],
    "test", null, 0,
  ), WebAssembly.RuntimeError, /./);
}

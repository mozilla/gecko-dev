load(libdir + "wasm-binary.js");

let test;
assertWarning(() => {
  test = wasmEvalBinary(moduleWithSections([
    sigSection([
      { args: [], ret: [] },
    ]),
    declSection([
      0,
    ]),
    exportSection([
      { funcIndex: 0, name: "test" },
    ]),
    bodySection([
      funcBody({ locals: [], body: [
        UnreachableCode
      ] }),
    ]),

    // This name section's contents declare themselves to be one byte longer
    // than they actually are. This can cause the decoder to read into adjacent
    // sections.
    nameSection([
      funcNameSubsection([
        { name: "1234567", nameLen: 8 }, // should be 7
      ], 11), // should be 10
    ]),

    // Without extra bytes at the end of the module, the decoder will EOF
    // before the name section is finished decoding.
    customSection("extra"),
  ])).exports.test;
}, /in the 'name' custom section: 1 bytes consumed past the end/);

try {
  test();
} catch (e) {
  print(e.stack);
  assertEq(e.stack.includes("1234567"), false);
}

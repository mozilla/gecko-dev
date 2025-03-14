// Check OOM handling during compilation of struct.new
function testNewStruct() {
  let { newStruct } = wasmEvalText(`
    (module
    (type $s (sub (struct)))
    (func (export "newStruct") (result anyref)
        struct.new $s)
    )
  `).exports;
}
oomTest(testNewStruct);

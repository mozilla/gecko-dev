// |jit-test| skip-if: !('oomTest' in this)

loadFile(`
  T = TypedObject
  ObjectStruct = new T.StructType({f: T.Object})
  var o = new ObjectStruct
  function testGC(p) {
    for (; i < 5; i++)
        whatever.push;
  }
  testGC(o)
  function writeObject() {
    return o.f = v;
  }
    writeObject({function() { } })
  for (var i ; i < 5 ; ++i)
    try {} catch (StringStruct) {}
`);
function loadFile(lfVarx) {
  oomTest(Function(lfVarx));
}

// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")

// Bug 1252103: Inline typed array objects need delayed metadata collection.
// Shouldn't crash.

function foo() {
    enableTrackAllocations();
    gczeal(2, 10);
    TO = TypedObject;
    PointType = new TO.StructType({
        y: TO.float64,
        name: TO.string
    })
    LineType = new TO.StructType({
        PointType
    })
    function testBasic() { return new LineType; }
    testBasic();
}
evaluate("foo()");


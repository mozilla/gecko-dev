var s = "\u1234".repeat(256);
var arr = [s, s, s, s, s, s, s, s];
oomTest(function() {
    serialize(arr);
});
var buf = serialize(arr);
oomTest(function() {
    deserialize(buf);
});

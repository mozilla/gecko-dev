function test() {
    var str = "()?".repeat(32767);
    var re = new RegExp(str);
    for (var i = 0; i < 10; i++) {
        var res = re.exec(str);
        assertEq(res.length, 32768);
        assertEq(res[0], "");
        assertEq(res[1], undefined);
        assertEq(res[32767], undefined);
    }
}
test();

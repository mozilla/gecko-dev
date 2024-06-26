function testNonCanonicalNan()
{
    const bytes = 128;
    var buf = new ArrayBuffer(bytes);

    /* create an array of non-canonical nans */
    var ui8arr = new Uint8Array(buf);
    ui8arr.fill(0xff);

    var dblarr = new Float64Array(buf);
    assertEq(dblarr.length, bytes / 8);

    /* ensure they are canonicalized */
    for (var i = 0; i < dblarr.length; ++i) {
        var asstr = dblarr[i] + "";
        var asnum = dblarr[i] + 0.0;
        assertEq(asstr, "NaN");
        assertEq(asnum, NaN);
    }

    var fltarr = new Float32Array(buf);
    assertEq(fltarr.length, bytes / 4);

    /* ensure they are canonicalized */
    for (var i = 0; i < fltarr.length; ++i) {
        var asstr = fltarr[i] + "";
        var asnum = fltarr[i] + 0.0;
        assertEq(asstr, "NaN");
        assertEq(asnum, NaN);
    }

    var flt16arr = new Float16Array(buf);
    assertEq(flt16arr.length, bytes / 2);

    /* ensure they are canonicalized */
    for (var i = 0; i < flt16arr.length; ++i) {
        var asstr = flt16arr[i] + "";
        var asnum = flt16arr[i] + 0.0;
        assertEq(asstr, "NaN");
        assertEq(asnum, NaN);
    }
}

testNonCanonicalNan();

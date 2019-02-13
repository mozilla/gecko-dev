/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


// The Worker constructor can take a relative URL, but different test runners
// run in different enough environments that it doesn't all just automatically
// work. For the shell, we use just a filename; for the browser, see browser.js.
var workerDir = '';

// explicitly turn on js185
// XXX: The browser currently only supports up to version 1.8
if (typeof version != 'undefined')
{
  version(185);
}


// Note that AsmJS ArrayBuffers have a minimum size, currently 4096 bytes. If a
// smaller size is given, a regular ArrayBuffer will be returned instead.
function AsmJSArrayBuffer(size) {
    var ab = new ArrayBuffer(size);
    (new Function('global', 'foreign', 'buffer', '' +
'        "use asm";' +
'        var i32 = new global.Int32Array(buffer);' +
'        function g() {};' +
'        return g;' +
''))(Function("return this")(),null,ab);
    return ab;
}

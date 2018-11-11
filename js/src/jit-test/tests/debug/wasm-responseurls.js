// |jit-test| test-also-no-wasm-baseline; skip-if: !wasmDebuggingIsSupported()
// Tests that wasm module can accept URL and sourceMapURL from response
// when instantiateStreaming is used.

try {
    WebAssembly.compileStreaming();
} catch (err) {
    assertEq(String(err).indexOf("not supported with --no-threads") !== -1, true);
    quit();
}

load(libdir + "asserts.js");

var g = newGlobal();

var source = new g.Uint8Array(wasmTextToBinary('(module (func unreachable))'));
source.url = "http://example.org/test.wasm";
source.sourceMappingURL = "http://example.org/test.wasm.map";
g.source = source;

var gotUrl, gotSourceMapURL;
var dbg = new Debugger(g);
dbg.allowWasmBinarySource = true;
dbg.onNewScript = function (s, g) {
    gotUrl = s.source.url;
    gotSourceMapURL = s.source.sourceMapURL;
};

g.eval('WebAssembly.instantiateStreaming(source);');

drainJobQueue();

assertEq(gotUrl, "http://example.org/test.wasm");
assertEq(gotSourceMapURL, "http://example.org/test.wasm.map");

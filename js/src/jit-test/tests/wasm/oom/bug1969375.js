ignoreUnhandledRejections();
function f() {
  drainJobQueue();
  WebAssembly.compileStreaming();
}
oomTest(f);

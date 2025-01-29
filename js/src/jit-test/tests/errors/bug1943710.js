// |jit-test| --ion-offthread-compile=off; --setpref=experimental.error_capture_stack_trace
if ('captureStackTrace' in Error) {
  run(`
    a16 = {};
    Error.captureStackTrace(a16, Error)
  `);
  function run(code) {
    evaluate(code)
  }
}

// META: global=window

"use strict";

test(t => {
  assert_throws_js(
    TypeError,
    () => new DecompressionStream("zstd"),
    'Constructor given "zstd" should throw.'
  );
}, '"zstd" should be an invalid DecompressionStream format in an unprivileged context.');

promise_test(async t => {
  const constructorSucceeded = await SpecialPowers.spawnChrome(
    [],
    function create_decompression_stream_in_chrome_context() {
      const ds = new this.browsingContext.topChromeWindow.DecompressionStream(
        "zstd"
      );

      if (!ds) {
        throw new Error(
          'Failed to construct DecompressionStream with "zstd" format in privileged context.'
        );
      }

      return true;
    }
  );

  assert_true(
    constructorSucceeded,
    'Constructor given "zstd" should succeed in a privileged context'
  );
}, '"zstd" should be a valid DecompressionStream format in a privileged context.');

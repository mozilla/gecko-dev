// META: global=window

"use strict";

test(t => {
  assert_throws_js(
    TypeError,
    () => new CompressionStream("zstd"),
    'Constructor given "zstd" should throw'
  );
}, '"zstd" should be an invalid CompressionStream option');

promise_test(async t => {
  const constructorFailed = await SpecialPowers.spawnChrome([], () => {
    let cs;

    try {
      cs = new this.browsingContext.topChromeWindow.CompressionStream("zstd");
    } catch {
      return true;
    }

    if (cs) {
      throw new Error(
        'Constructed a CompressionStream with "zstd" format, when that should be disallowed'
      );
    }
  });

  assert_true(
    constructorFailed,
    'Constructor given "zstd" should throw, even in a privileged context'
  );
}, '"zstd" should be an invalid CompressionStream format, even in a privileged context');

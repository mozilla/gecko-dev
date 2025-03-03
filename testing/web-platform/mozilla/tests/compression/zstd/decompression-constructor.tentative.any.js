// META: global=window

"use strict";

promise_test(async t => {
  t.add_cleanup(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.compression_streams.zstd.enabled", false]],
  });

  assert_throws_js(
    TypeError,
    () => new DecompressionStream("zstd"),
    'Constructor given "zstd" should throw'
  );
}, '"zstd" should be an invalid DecompressionStream format in an unprivileged context with the pref set to false');

promise_test(async t => {
  t.add_cleanup(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.compression_streams.zstd.enabled", true]],
  });

  const ds = new DecompressionStream("zstd");
  assert_equals(ds.constructor.name, "DecompressionStream", "");
}, '"zstd" should be a valid DecompressionStream format in an unprivileged context with the pref set to true');

promise_test(async t => {
  t.add_cleanup(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.compression_streams.zstd.enabled", true]],
  });

  const constructorSucceeded = await SpecialPowers.spawnChrome(
    [],
    () => {
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
}, '"zstd" should be a valid DecompressionStream format in a privileged context with the pref set to false');

promise_test(async t => {
  t.add_cleanup(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.compression_streams.zstd.enabled", true]],
  });

  const constructorSucceeded = await SpecialPowers.spawnChrome(
    [],
    () => {
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
}, '"zstd" should be a valid DecompressionStream format in a privileged context with the pref set to true');

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
    () => new CompressionStream("zstd"),
    'Constructor given "zstd" should throw'
  );
}, '"zstd" should be an invalid CompressionStream format in an unprivileged context with the pref set to false');

promise_test(async t => {
  t.add_cleanup(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.compression_streams.zstd.enabled", true]],
  });

  assert_throws_js(
    TypeError,
    () => new CompressionStream("zstd"),
    'Constructor given "zstd" should throw'
  );
}, '"zstd" should be an invalid CompressionStream format in an unprivileged context with the pref set to true');

promise_test(async t => {
  t.add_cleanup(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.compression_streams.zstd.enabled", false]],
  });

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
}, '"zstd" should be an invalid CompressionStream format in a privileged context with the pref set to false');

promise_test(async t => {
  t.add_cleanup(async () => {
    await SpecialPowers.popPrefEnv();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["dom.compression_streams.zstd.enabled", true]],
  });

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
}, '"zstd" should be an invalid CompressionStream format in a privileged context with the pref set to true');

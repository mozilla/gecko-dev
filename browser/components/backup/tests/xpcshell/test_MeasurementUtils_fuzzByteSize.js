/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_minimumFallback() {
  const fuzzed = MeasurementUtils.fuzzByteSize(250, 1000);
  Assert.equal(
    fuzzed,
    1000,
    "Should fall back to the `nearest` value when `bytes` are below `nearest`"
  );
});

add_task(async function test_roundUp() {
  const fuzzed = MeasurementUtils.fuzzByteSize(1500, 1000);
  Assert.equal(
    fuzzed,
    2000,
    "Should round up to 2000 when `bytes` is 1500 since that is the nearest 1000 bytes"
  );
});

add_task(async function test_roundDown() {
  const fuzzed = MeasurementUtils.fuzzByteSize(1499, 1000);
  Assert.equal(
    fuzzed,
    1000,
    "Should round down to 1000 when `bytes` is 1499 since that is the nearest 1000 bytes"
  );
});

add_task(async function test_roundDownSmallerUnit() {
  const fuzzed = MeasurementUtils.fuzzByteSize(1025, 10);
  Assert.equal(
    fuzzed,
    1030,
    "Should round 1025 up to 1030 since that is the nearest 10 bytes"
  );
});

add_task(async function test_roundDownSmallerUnit() {
  const fuzzed = MeasurementUtils.fuzzByteSize(1024, 10);
  Assert.equal(
    fuzzed,
    1020,
    "Should round 1024 down to 1020 since that is the nearest 10 bytes"
  );
});

add_task(async function test_roundUpBinary() {
  const fuzzed = MeasurementUtils.fuzzByteSize(1500, 1024);
  Assert.equal(
    fuzzed,
    1024,
    "Should round 1500 down to 1024 nearest kibibyte value"
  );
});

add_task(async function test_roundDownBinary() {
  const fuzzed = MeasurementUtils.fuzzByteSize(1800, 1024);
  Assert.equal(
    fuzzed,
    2048,
    "Should round 1800 up to 2048 since that is the nearest kibibyte value"
  );
});

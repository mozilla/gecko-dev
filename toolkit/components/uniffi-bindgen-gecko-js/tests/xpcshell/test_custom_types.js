/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {
  roundtripCustomType,
  roundtripUrl,
  roundtripTimeIntervalMs,
  roundtripTimeIntervalSecDbl,
  roundtripTimeIntervalSecFlt,
  getCustomTypesDemo,
} = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/tests/generated/RustUniffiBindingsTests.sys.mjs"
);

// Test Handle custom type (already exists)
Assert.equal(roundtripCustomType(100), 100);

// Test URL custom type
add_task(async function testUrlCustomType() {
  const testUrl = new URL("https://example.com/path?query=value#fragment");
  const result = await roundtripUrl(testUrl);

  Assert.equal(result.href, testUrl.href, "Round-tripped URL should match");
  Assert.equal(result.toString(), testUrl.toString());
  Assert.equal(result.hostname, "example.com");
  Assert.equal(result.pathname, "/path");
  Assert.equal(result.search, "?query=value");
  Assert.equal(result.hash, "#fragment");
});

// Test TimeIntervalMs custom type (milliseconds to Date)
add_task(async function testTimeIntervalMsCustomType() {
  const now = new Date();
  const result = await roundtripTimeIntervalMs(now);

  Assert.equal(
    Object.prototype.toString.call(result),
    "[object Date]",
    "Result should be a Date instance"
  );
  Assert.equal(result.getTime(), now.getTime());
});

// Test TimeIntervalSecDbl custom type (seconds as float to Date)
add_task(async function testTimeIntervalSecDblCustomType() {
  const now = new Date();
  const result = await roundtripTimeIntervalSecDbl(now);

  Assert.equal(
    Object.prototype.toString.call(result),
    "[object Date]",
    "Result should be a Date instance"
  );
  Assert.ok(
    Math.abs(result.getTime() - now.getTime()) < 1,
    "Times should be very close"
  );
});

// Test all custom types together
add_task(async function testCustomTypesDemo() {
  const demo = await getCustomTypesDemo();

  Assert.equal(demo.url.toString(), "https://example.com/");

  // Check Handle
  Assert.equal(typeof demo.handle, "number");
  Assert.equal(demo.handle, 123);

  // Check TimeIntervalMs
  Assert.equal(
    Object.prototype.toString.call(demo.timeIntervalMs),
    "[object Date]",
    "timeIntervalMs should be a Date instance"
  );
  Assert.equal(demo.timeIntervalMs.getTime(), 456000);

  // Check TimeIntervalSecDbl
  Assert.equal(
    Object.prototype.toString.call(demo.timeIntervalSecDbl),
    "[object Date]",
    "timeIntervalSecDbl should be a Date instance"
  );
  Assert.equal(demo.timeIntervalSecDbl.getTime(), 456000); // 456.0 seconds = 456000 ms
});

// Test error handling for invalid URLs
add_task(async function testInvalidUrl() {
  await Assert.rejects(
    roundtripUrl("not a valid url"),
    /TypeError/,
    "Should throw TypeError for invalid URL strings"
  );
});

// Test edge cases for dates
add_task(async function testDateEdgeCases() {
  const epoch = new Date(0);
  Assert.equal((await roundtripTimeIntervalMs(epoch)).getTime(), 0);

  const beforeEpoch = new Date(-1000);
  Assert.equal((await roundtripTimeIntervalMs(beforeEpoch)).getTime(), -1000);

  const farFuture = new Date(2147483647000); // Near max 32-bit timestamp in ms
  Assert.equal(
    (await roundtripTimeIntervalMs(farFuture)).getTime(),
    2147483647000
  );
});

add_task(async function testConfigPresenceComparison() {
  // This test demonstrates the difference between types WITH and WITHOUT config

  // TimeIntervalSecDbl HAS config - should work with Dates
  const testDate = new Date(2024, 0, 1);
  const dblResult = await roundtripTimeIntervalSecDbl(testDate);

  Assert.equal(
    Object.prototype.toString.call(dblResult),
    "[object Date]",
    "Result should be a Date instance"
  );

  // Demonstrate the failure case
  await Assert.rejects(
    roundtripTimeIntervalSecDbl("NOT A DATE"), // Date to no-config type
    /TypeError/,
    "Passing Date to sconfig type should fail"
  );

  // TimeIntervalSecFlt has NO config - should work with numbers only
  const testSeconds = testDate.getTime() / 1000;
  const fltResult = await roundtripTimeIntervalSecFlt(testSeconds);
  Assert.equal(
    typeof fltResult,
    "number",
    "TimeIntervalSecFlt (without config) should return numbers"
  );
});

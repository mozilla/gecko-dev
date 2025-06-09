/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { _FilterAdult, FilterAdult } = ChromeUtils.importESModule(
  "resource:///modules/FilterAdult.sys.mjs"
);

const { FilterAdultComponent } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/uniffi-bindgen-gecko-js/components/generated/RustFilterAdult.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

let originalPrefValue;
const FILTER_ADULT_ENABLED_PREF =
  "browser.newtabpage.activity-stream.filterAdult";
const TEST_ADULT_SITE_URL = "https://some-adult-site.com/";
const TEST_ADULT_SITE_BASE_DOMAIN = Services.eTLD.getBaseDomain(
  Services.io.newURI(TEST_ADULT_SITE_URL)
);
const TEST_NON_ADULT_SITE_URL = "https://mozilla.org/";

/**
 * Instantiates a new instance of _FilterAdult, but wrapping a stubbed out
 * version of the FilterAdultComponent Rust component that always returns false
 * for the "contains" call _except_ when that call passes
 * TEST_ADULT_SITE_BASE_DOMAIN.
 *
 * @param {SinonSandbox} sandbox
 *   A sinon sandbox that will be used to generate stubs.
 * @param {function(_FilterAdult, SinonStub)} taskFn
 *   A synchronous testing function that will be called with the fake instance
 *   of FilterAdult, as well as the stub for the contains call on the fake
 *   Rust component, so that callers can check if the stub is ever called.
 */
function withFakeFilterAdult(sandbox, taskFn) {
  let fakeRustComponent = {
    contains: sandbox.stub(),
  };

  sandbox.stub(FilterAdultComponent, "init").returns(fakeRustComponent);

  // Now instantiate a new instance of _FilterAdult so that it uses our
  // fake component on construction.
  let instance = new _FilterAdult();

  // Pretend that TEST_ADULT_SITE_URL is an adult website, but all other sites
  // are not.
  fakeRustComponent.contains.returns(false);
  fakeRustComponent.contains
    .withArgs(TEST_ADULT_SITE_BASE_DOMAIN)
    .returns(true);

  taskFn(instance, fakeRustComponent);
}

/**
 * Tests that filtering doesn't strip out objects that don't include the
 * `url` property.
 */
add_task(async function test_defaults_to_include_unexpected_urls() {
  const empty = {};
  const result = FilterAdult.filter([empty]);
  Assert.equal(result.length, 1, "Did not filter any items.");
  Assert.equal(result[0], empty, "The items were not changed.");
});

/**
 * Tests that filtering doesn't strip out objects that include non-adult
 * URLs for the `url` property.
 */
add_task(async function test_does_not_filter_non_adult_urls() {
  const link = { url: TEST_NON_ADULT_SITE_URL };
  const result = FilterAdult.filter([link]);
  Assert.equal(result.length, 1, "Did not filter any items.");
  Assert.equal(result[0], link, "The items were unchanged.");
});

/**
 * Tests that filtering will call into the underlying FilterAdultComponent
 * Rust component, and that if that component computes that the URL is
 * an adult URL, that it gets filtered out.
 */
add_task(async function test_filters_out_adult_urls() {
  let sandbox = sinon.createSandbox();

  withFakeFilterAdult(sandbox, instance => {
    const link = { url: TEST_ADULT_SITE_URL };
    const result = instance.filter([link]);
    Assert.equal(result.length, 0, "Filtered an adult site link.");
  });

  sandbox.restore();
});

/**
 * Tests that filtering will not filter anything (adult site or otherwise) when
 * the filtering pref is false. The test will ensure that the underlying Rust
 * component is never actually called when the pref is false.
 */
add_task(async function test_does_not_filter_adult_urls_when_pref_off() {
  let sandbox = sinon.createSandbox();
  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, false);

  withFakeFilterAdult(sandbox, (instance, fakeRustComponent) => {
    const link = { url: TEST_ADULT_SITE_URL };
    const result = instance.filter([link]);
    Assert.equal(
      result.length,
      1,
      "Did not filter an adult site when disabled."
    );
    Assert.equal(
      result[0],
      link,
      "Did not alter an adult site link when disabled."
    );
    Assert.ok(
      fakeRustComponent.contains.notCalled,
      "Rust component was not called when disabled."
    );
  });

  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, true);
  sandbox.restore();
});

/**
 * Tests that isAdultUrl will return false for things that aren't technically
 * URLs.
 */
add_task(async function test_isAdultUrl_returns_false_for_unexpected_urls() {
  const result = FilterAdult.isAdultUrl("");
  Assert.ok(!result, "Invalid URLs are not considered adult.");
});

/**
 * Tests that isAdultUrl will return false for things that are not
 * adult URLs.
 */
add_task(async function test_isAdultUrl_returns_false_for_non_adult_urls() {
  const result = FilterAdult.isAdultUrl(TEST_NON_ADULT_SITE_URL);
  Assert.ok(!result, "Non-adult sites are not considered adult.");
});

/**
 * Tests that isAdultUrl will return true for things that the underlying
 * Rust component considers an adult URL.
 */
add_task(async function test_isAdultUrl_returns_true_for_adult_urls() {
  let sandbox = sinon.createSandbox();
  withFakeFilterAdult(sandbox, instance => {
    const result = instance.isAdultUrl(TEST_ADULT_SITE_URL);
    Assert.ok(result, "Site was classified as being for adults.");
  });
  sandbox.restore();
});

/**
 * Tests that isAdultUrl will not return true when the filtering pref is false,
 * even if passed an adult site. The test will ensure that the underlying Rust
 * component is never actually called when the pref is false.
 */
add_task(async function test_isAdultUrl_returns_false_when_pref_off() {
  let sandbox = sinon.createSandbox();
  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, false);

  withFakeFilterAdult(sandbox, (instance, fakeRustComponent) => {
    const result = instance.isAdultUrl(TEST_ADULT_SITE_URL);
    Assert.ok(
      !result,
      "Site was NOT classified as being for adults because filtering is disabled."
    );
    Assert.ok(
      fakeRustComponent.contains.notCalled,
      "Rust component was not called when disabled."
    );
  });

  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, true);
  sandbox.restore();
});

/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * @typedef {object} TestRenderTemplateResult
 * @property {string} markup
 *   The rendered template markup as a string.
 * @property {Document} backupDOM
 *   The rendered template as parsed by a DOMParser.
 */

/**
 * Renders a template and returns an object that contains both the raw markup
 * and the DOM of the markup as parsed by DOMParser.
 *
 * @param {boolean} isEncrypted
 *   True if the template should report that the backup is encrypted.
 * @param {object} metadata
 *   The metadata for the backup. See the BackupManifest schema for details.
 * @returns {TestRenderTemplateResult}
 */
async function testRenderTemplate(isEncrypted, metadata = FAKE_METADATA) {
  let bs = new BackupService();
  let markup = await bs.renderTemplate(
    BackupService.ARCHIVE_TEMPLATE,
    isEncrypted,
    metadata
  );
  let backupDOM = new DOMParser().parseFromString(markup, "text/html");
  return { backupDOM, markup };
}

add_setup(() => {
  // Setting this pref lets us use Cu.evalInSandbox to run the archive.js
  // script.
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_parent_unrestricted_js_loads");
  });
});

/**
 * Tests that header matches our expectations. The header of this file should
 * be:
 *
 * <!DOCTYPE html>
 * <!-- Version: 1 -->
 */
add_task(async function test_header() {
  let { markup } = await testRenderTemplate(false /* isEncrypted */);
  const EXPECTED_HEADER =
    /^<!DOCTYPE html>[\r\n]+<!-- Version: (\d+) -->[\r\n]+/;
  Assert.ok(
    markup.match(EXPECTED_HEADER),
    "Should have found the expected header."
  );
});

/**
 * Tests that the `encryption-state` DOM node says "Not encrypted" if the backup
 * is not encrypted, and "Encrypted" otherwise.
 */
add_task(async function test_encryption_state() {
  let { backupDOM } = await testRenderTemplate(false /* isEncrypted */);
  Assert.equal(
    backupDOM.querySelector("#encryption-state").textContent,
    "Not encrypted"
  );

  ({ backupDOM } = await testRenderTemplate(true /* isEncrypted */));
  Assert.equal(
    backupDOM.querySelector("#encryption-state").textContent,
    "Encrypted"
  );
});

/**
 * Tests that metadata is properly inserted. The expected metadata inserted on
 * the page is the time and date of the backup, as well as the name of the
 * machine that the backup was created on.
 */
add_task(async function test_metadata() {
  let { backupDOM } = await testRenderTemplate(true /* isEncrypted */);
  let backupDate = new Date(FAKE_METADATA.date);
  let expectedDate = new Intl.DateTimeFormat("en-US", {
    dateStyle: "short",
  }).format(backupDate);
  let expectedTime = new Intl.DateTimeFormat("en-US", {
    timeStyle: "short",
  }).format(backupDate);
  Assert.equal(
    backupDOM.querySelector("#last-backed-up").textContent,
    `Last backed up: ${expectedTime}, ${expectedDate}`
  );
  Assert.equal(
    backupDOM.querySelector("#creation-device").textContent,
    "Created on A super cool machine"
  );
});

/**
 * Tests that metadata is properly escaped. This isn't exhaustive, since we're
 * using Fluent under the hood, which is tested pretty widely already.
 */
add_task(async function test_hostile_metadata() {
  let { backupDOM } = await testRenderTemplate(true /* isEncrypted */, {
    date: "<script>alert('test');</script>",
    appName: "<script>alert('test');</script>",
    appVersion: "<script>alert('test');</script>",
    buildID: "<script>alert('test');</script>",
    profileName: "<script>alert('test');</script>",
    machineName: "<script>alert('test');</script>",
    osName: "<script>alert('test');</script>",
    osVersion: "<script>alert('test');</script>",
    legacyClientID: "<script>alert('test');</script>",
    profileGroupID: "<script>alert('test');</script>",
    accountID: "<script>alert('test');</script>",
    accountEmail: "<script>alert('test');</script>",
  });

  let scriptTags = backupDOM.querySelectorAll("script");
  Assert.equal(
    scriptTags.length,
    1,
    "There should only be 1 script tag on the page."
  );
  let scriptContent = scriptTags[0].innerHTML;
  let evalSandbox = Cu.Sandbox(Cu.getGlobalForObject({}));

  evalSandbox.navigator = {
    userAgent: "",
  };
  evalSandbox.document = {
    body: {
      toggleAttribute: sinon.stub(),
    },
  };
  evalSandbox.alert = sinon.stub();

  Cu.evalInSandbox(scriptContent, evalSandbox);

  Assert.ok(evalSandbox.alert.notCalled, "alert() was never called");
});

/**
 * Tests that if the User Agent is a browser that includes "Firefox", that
 * toggleAttribute("is-moz-browser", true) is called on document.body.
 */
add_task(async function test_moz_browser_handling() {
  let { backupDOM } = await testRenderTemplate(false /* isEncrypted */);
  let scriptTags = backupDOM.querySelectorAll("script");
  Assert.equal(
    scriptTags.length,
    1,
    "There should only be 1 script tag on the page."
  );
  let scriptContent = scriptTags[0].innerHTML;
  let evalSandbox = Cu.Sandbox(Cu.getGlobalForObject({}));

  evalSandbox.navigator = {
    userAgent:
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:129.0) Gecko/20100101 Firefox/129.0",
  };
  evalSandbox.document = {
    body: {
      toggleAttribute: sinon.stub(),
    },
  };

  Cu.evalInSandbox(scriptContent, evalSandbox);

  Assert.ok(
    evalSandbox.document.body.toggleAttribute.calledOnce,
    "document.body.toggleAttribute called"
  );
  Assert.ok(
    evalSandbox.document.body.toggleAttribute.calledWith(
      "is-moz-browser",
      true
    ),
    "document.body.toggleAttribute called setting is-moz-browser to true"
  );
});

/**
 * Tests that if the User Agent is a browser that does not include "Firefox",
 * that toggleAttribute("is-moz-browser", false) is called on document.body.
 */
add_task(async function test_non_moz_browser_handling() {
  let { backupDOM } = await testRenderTemplate(true /* isEncrypted */);
  let scriptTags = backupDOM.querySelectorAll("script");
  Assert.equal(
    scriptTags.length,
    1,
    "There should only be 1 script tag on the page."
  );
  let scriptContent = scriptTags[0].innerHTML;
  let evalSandbox = Cu.Sandbox(Cu.getGlobalForObject({}));

  evalSandbox.navigator = {
    userAgent:
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4.1 Safari/605.1.15",
  };
  evalSandbox.document = {
    body: {
      toggleAttribute: sinon.stub(),
    },
  };

  Cu.evalInSandbox(scriptContent, evalSandbox);

  Assert.ok(
    evalSandbox.document.body.toggleAttribute.calledOnce,
    "document.body.toggleAttribute called"
  );
  Assert.ok(
    evalSandbox.document.body.toggleAttribute.calledWith(
      "is-moz-browser",
      false
    ),
    "document.body.toggleAttribute called setting is-moz-browser to false"
  );
});

/**
 * Tests that the license header does not exist in the generated rendering.
 */
add_task(async function test_no_license() {
  let { markup } = await testRenderTemplate(true /* isEncrypted */);

  // Instead of looking for the exact license header (which might be indented)
  // in such a way as to make string-searching brittle) we'll just look for
  // a key part of the license header, which is the reference to
  // https://mozilla.org/MPL.

  Assert.ok(
    !markup.includes("https://mozilla.org/MPL"),
    "The license headers were stripped."
  );
});

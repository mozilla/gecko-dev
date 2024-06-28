/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BackupService } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupService.sys.mjs"
);

const { MockFilePicker } = SpecialPowers;

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const MOCK_PASSWORD = "mckP@ss3x2 fake_password";

/**
 * Dispatches an input event for a password input field.
 *
 * @param {HTMLElement} inputEl
 *  the input element that will dispatch the input event
 * @param {string} mockPassword
 *  the password entered for the input element
 * @returns {Promise<undefined>}
 */
function createMockPassInputEventPromise(inputEl, mockPassword) {
  let promise = new Promise(resolve => {
    inputEl.addEventListener("input", () => resolve(), {
      once: true,
    });
  });
  inputEl.focus();
  inputEl.value = mockPassword;
  inputEl.dispatchEvent(new Event("input"));
  return promise;
}

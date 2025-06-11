/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "CredentialChooserService",
  "@mozilla.org/browser/credentialchooserservice;1",
  "nsICredentialChooserService"
);

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

const TEST_URL = "https://www.example.com/";
const TEST_URL_2 = "https://example.com/";
const TEST_CREDENTIAL_URL = "https://www.example.net/";
const TEST_IMAGE_DATA =
  "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciCiAgICAgd2lkdGg9IjMyIiBoZWlnaHQ9IjMyIiB2aWV3Qm94PSIwIDAgMzIgMzIiPgogIDxjaXJjbGUgZmlsbD0iY29udGV4dC1maWxsIiBjeD0iMTYiIGN5PSIxNiIgcj0iMTYiLz4KPC9zdmc+";

// A single credential shows up in a dialog and is chosen when "continue" is clicked
add_task(async function test_single_credential_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credential = { id: "foo", type: "identity", origin: TEST_CREDENTIAL_URL };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      [credential],
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Validate the popup contents
  let inputs = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 1, "One account expected");
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label.textContent.includes("www.example.net"),
    "Label includes the credential's origin without UI hints"
  );
  let title = document.getElementById("credential-chooser-header-text");
  ok(
    title.textContent.includes("www.example.com"),
    "Popup title includes the visited hostname"
  );
  let icon = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item-icon")[0];
  is(
    icon.src,
    "chrome://global/skin/icons/defaultFavicon.svg",
    "The icon matches the default favicon."
  );

  // Click
  EventUtils.synthesizeMouseAtCenter(
    document.getElementsByClassName("popup-notification-primary-button")[0],
    {}
  );

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(choice, "foo", "must have the correct credential chosen");

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// A null id is selected when "cancel" is clicked
add_task(async function test_dialog_cancel() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credential = { id: "foo", type: "identity", origin: TEST_CREDENTIAL_URL };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      [credential],
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Click
  EventUtils.synthesizeMouseAtCenter(
    document.getElementsByClassName("popup-notification-secondary-button")[0],
    {}
  );

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(choice, null, "Cancel should give a null choice");

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// Dismissing the prompt resolves the callback with null
add_task(async function test_dialog_dismiss() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credential = { id: "foo", type: "identity", origin: TEST_CREDENTIAL_URL };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      [credential],
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Press escape
  EventUtils.synthesizeKey("KEY_Escape");

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(choice, null, "Escape should give a null choice");

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// Three credentials are in the dialog and are rendered and interact correctly
add_task(async function test_three_credential_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credentials = [
    { id: "foo", type: "identity", origin: "https://www.example.net/" },
    { id: "bar", type: "identity", origin: "https://example.net/" },
    { id: "baz", type: "identity", origin: "https://example.com/" },
  ];

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      credentials,
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Validate the popup contents

  let inputs = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item");
  is(inputs.length, 3, "Three accounts expected");
  let title = document.getElementById("credential-chooser-header-text");
  ok(
    title.textContent.includes("www.example.com"),
    "Popup title includes the visited hostname"
  );
  const LABELS = ["www.example.net", "example.net", "example.com"];
  for (let index = 0; index < inputs.length; index++) {
    let input = inputs[index];
    let label = input.getElementsByClassName(
      "identity-credential-list-item-label-stack"
    )[0];
    ok(
      label.textContent.includes(LABELS[index]),
      "Label includes the credential's origin without UI hints"
    );
    let radio = input.getElementsByClassName(
      "identity-credential-list-item-radio"
    )[0];
    is(radio.checked, index == 0, "Initial radio state correct");
    EventUtils.synthesizeMouseAtCenter(input, {});
    ok(radio.checked, "Radio state selected after click");
  }

  // Click ok.
  EventUtils.synthesizeMouseAtCenter(
    document.getElementsByClassName("popup-notification-primary-button")[0],
    {}
  );

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(
    choice,
    "baz",
    "must have the correct credential chosen (the last of the list)"
  );

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// A credential shows up with UI hints when they never expire
add_task(async function test_uihint_nonexpiring_credential_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credential = {
    id: "foo",
    type: "identity",
    origin: TEST_CREDENTIAL_URL,
    uiHints: {
      iconURL: TEST_IMAGE_DATA,
      name: "user readable",
    },
  };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      [credential],
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Validate the popup contents

  let inputs = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item");
  let title = document.getElementById("credential-chooser-header-text");
  ok(
    title.textContent.includes("www.example.com"),
    "Popup title includes the visited hostname"
  );
  is(inputs.length, 1, "One account expected");
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label.textContent.includes("www.example.net"),
    "Label still includes the credential's origin with UI hints"
  );
  ok(
    label.textContent.includes("user readable"),
    "Label includes the credential's name with UI hints"
  );
  let icon = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item-icon")[0];
  is(icon.src, TEST_IMAGE_DATA, "The icon matches the custom.svg");

  // Click
  EventUtils.synthesizeMouseAtCenter(
    document.getElementsByClassName("popup-notification-primary-button")[0],
    {}
  );

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(choice, "foo", "must have the correct credential chosen");

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// A credential shows up with UI hints when they expire in the future
add_task(async function test_uihint_nonexpiring_credential_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credential = {
    id: "foo",
    type: "identity",
    origin: TEST_CREDENTIAL_URL,
    uiHints: {
      iconURL: TEST_IMAGE_DATA,
      name: "user readable",
      expiresAfter: 1000,
    },
  };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      [credential],
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Validate the popup contents

  let inputs = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item");
  let title = document.getElementById("credential-chooser-header-text");
  ok(
    title.textContent.includes("www.example.com"),
    "Popup title includes the visited hostname"
  );
  is(inputs.length, 1, "One account expected");
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label.textContent.includes("www.example.net"),
    "Label still includes the credential's origin with UI hints"
  );
  ok(
    label.textContent.includes("user readable"),
    "Label includes the credential's name with UI hints"
  );
  let icon = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item-icon")[0];
  is(icon.src, TEST_IMAGE_DATA, "The icon matches the custom.svg");

  // Click
  EventUtils.synthesizeMouseAtCenter(
    document.getElementsByClassName("popup-notification-primary-button")[0],
    {}
  );

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(choice, "foo", "must have the correct credential chosen");

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// A credential shows up with UI hints when they never expire
add_task(async function test_uihint_not_yet_expiring_credential_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credential = {
    id: "foo",
    type: "identity",
    origin: TEST_CREDENTIAL_URL,
    uiHints: {
      iconURL: TEST_IMAGE_DATA,
      name: "user readable",
    },
  };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      [credential],
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Validate the popup contents

  let inputs = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item");
  let title = document.getElementById("credential-chooser-header-text");
  ok(
    title.textContent.includes("www.example.com"),
    "Popup title includes the visited hostname"
  );
  is(inputs.length, 1, "One account expected");
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label.textContent.includes("www.example.net"),
    "Label still includes the credential's origin with UI hints"
  );
  ok(
    label.textContent.includes("user readable"),
    "Label includes the credential's name with UI hints"
  );
  let icon = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item-icon")[0];
  is(icon.src, TEST_IMAGE_DATA, "The icon matches the custom.svg");

  // Click
  EventUtils.synthesizeMouseAtCenter(
    document.getElementsByClassName("popup-notification-primary-button")[0],
    {}
  );

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(choice, "foo", "must have the correct credential chosen");

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// A credential shows up without UI hints when they are expired
add_task(async function test_uihint_expired_credential_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Construct our credential to choose from
  let credential = {
    id: "foo",
    type: "identity",
    origin: TEST_CREDENTIAL_URL,
    uiHints: {
      iconURL: TEST_IMAGE_DATA,
      name: "user readable",
      expiresAfter: 0,
    },
  };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(
      tab.linkedBrowser.browsingContext,
      [credential],
      {
        notify: resolve,
      }
    );
  });

  // Wait for the popup to appear
  await popupShown;
  let popupHiding = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphiding"
  );

  // Validate the popup contents

  let inputs = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item");
  let title = document.getElementById("credential-chooser-header-text");
  ok(
    title.textContent.includes("www.example.com"),
    "Popup title includes the visited hostname"
  );
  is(inputs.length, 1, "One account expected");
  let label = inputs[0].getElementsByClassName(
    "identity-credential-list-item-label-stack"
  )[0];
  ok(
    label.textContent.includes("www.example.net"),
    "Label still includes the credential's origin"
  );
  ok(
    !label.textContent.includes("user readable"),
    "Label does not include the credential's name without UI hints"
  );
  let icon = document
    .getElementById("credential-chooser-entry")
    .getElementsByClassName("identity-credential-list-item-icon")[0];
  Assert.notEqual(icon.src, TEST_IMAGE_DATA, "We don't use the hint icon");

  // Click
  EventUtils.synthesizeMouseAtCenter(
    document.getElementsByClassName("popup-notification-primary-button")[0],
    {}
  );

  // Wait for the pop to go away
  await popupHiding;

  let choice = await credentialChoice;
  is(choice, "foo", "must have the correct credential chosen");

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

// Inactive documents should not have a popup.
add_task(async function test_inavtive_document_doesnt_show_dialog() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let oldBC = tab.linkedBrowser.browsingContext;

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, TEST_URL_2);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, TEST_URL_2);

  // Construct our credential to choose from
  let credential = { id: "foo", type: "identity", origin: TEST_CREDENTIAL_URL };

  // Show the single account
  let credentialChoice = new Promise(resolve => {
    CredentialChooserService.showCredentialChooser(oldBC, [credential], {
      notify: resolve,
    });
  });

  let choice = await credentialChoice;
  is(
    choice,
    null,
    "An inactive should give a null choice without showing a popup"
  );

  // Clear the tab
  await BrowserTestUtils.removeTab(tab);
});

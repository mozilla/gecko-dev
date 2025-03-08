/* global add_autofill_heuristic_tests */

"use strict";

// This test verifies that the correct events get fired when forms
// are nested or the form attribute is used on an element.

const TEST_PROFILE = {
  "given-name": "Jim",
  "family-name": "Walker",
  "street-address": "160 Main St\nApartment 160",
  "postal-code": "91111",
  email: "jim@example.com",
};

function testStart(idToFocus) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser.browsingContext,
    [idToFocus],
    async idToFocusChild => {
      content.document
        .getElementById("mainform")
        .appendChild(content.document.getElementById("subform"));
      content.document.getElementById(idToFocusChild).focus();
    }
  );
}

add_autofill_heuristic_tests(
  [
    {
      description:
        "<form> with a nested <form>; parser should strip the nested form out",
      fixtureData: `<form id="mainform">
                      <label for="street1">Address line 1</label>
                      <input type="text" id="street1">
                      <label for="street2">Address line 2</label>
                      <input type="text" id="street2">
                      <form id="subform">
                        <label for="postcode">Postcode</label>
                        <input type="search" id="postcode">
                      </form>
                    </form>`,
      profile: TEST_PROFILE,
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            {
              fieldName: "address-line1",
              autofill: TEST_PROFILE["street-address"].split("\n")[0],
            },
            {
              fieldName: "address-line2",
              autofill: TEST_PROFILE["street-address"].split("\n")[1],
              reason: "update-heuristic",
            },
            { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          ],
        },
      ],
    },
    {
      description: "<form> where a second <form> is moved inside it",
      fixtureData: `<form id="mainform">
                      <label for="street1">Address line 1</label>
                      <input type="text" id="street1">
                      <label for="street2">Address line 2</label>
                      <input type="text" id="street2">
                    </form>
                    <form id="subform">
                      <label for="postcode">Postcode</label>
                      <input type="search" id="postcode">
                    </form>`,
      profile: TEST_PROFILE,
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            {
              fieldName: "address-line1",
              autofill: TEST_PROFILE["street-address"].split("\n")[0],
            },
            {
              fieldName: "address-line2",
              autofill: TEST_PROFILE["street-address"].split("\n")[1],
              reason: "update-heuristic",
            },
            { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          ],
        },
        {
          invalid: true,
          fields: [
            {
              fieldName: "postal-code",
              autofill: TEST_PROFILE["postal-code"],
              reason: "regex-heuristic",
            },
          ],
        },
      ],
      onTestStart: () => testStart("street1"),
    },
    {
      description:
        "<form> where a second <form> is moved inside it where a field in the second form is focused",
      fixtureData: `<form id="mainform">
                      <label for="street1">Address line 1</label>
                      <input type="text" id="street1">
                      <label for="street2">Address line 2</label>
                      <input type="text" id="street2">
                    </form>
                    <form id="subform">
                      <label for="postcode">Postcode</label>
                      <input type="search" id="postcode">
                    </form>`,
      profile: TEST_PROFILE,
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            {
              fieldName: "address-line1",
              autofill: TEST_PROFILE["street-address"].split("\n")[0],
            },
            {
              fieldName: "address-line2",
              autofill: TEST_PROFILE["street-address"].split("\n")[1],
              reason: "update-heuristic",
            },
            { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          ],
        },
        {
          invalid: true,
          fields: [
            {
              fieldName: "postal-code",
              autofill: TEST_PROFILE["postal-code"],
              reason: "regex-heuristic",
            },
          ],
        },
      ],
      onTestStart: () => testStart("postcode"),
    },
    {
      description:
        "<form> with a field with a form attribute pointing to a second form",
      fixtureData: `<form id="mainform">
                      <label for="street1">Address line 1</label>
                      <input type="text" id="street1">
                      <label for="street2">Address line 2</label>
                      <input type="text" id="street2">
                      <label for="postcode">Postcode</label>
                      <input type="search" id="postcode" form="subform">
                    </form>
                    <form id="subform">
                      <label for="firstname">First Name</label>
                      <input type="text" id="firstname">
                      <label for="lastname">Last Name</label>
                      <input type="text" id="lastname">
                    </form>`,
      profile: TEST_PROFILE,
      autofillTrigger: "#postcode",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          invalid: true,
          fields: [
            { fieldName: "address-line1" },
            { fieldName: "address-line2", reason: "update-heuristic" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
            { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
            { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          ],
        },
      ],
    },
    {
      description: "field outside a <form> with a form attribute",
      fixtureData: `<form id="mainform">
                      <label for="street1">Address line 1</label>
                      <input type="text" id="street1">
                      <label for="street2">Address line 2</label>
                      <input type="text" id="street2">
                    </form>
                    <form id="subform">
                      <label for="firstname">First Name</label>
                      <input type="text" id="firstname">
                      <label for="lastname">Last Name</label>
                      <input type="text" id="lastname">
                    </form>
                    <label for="postcode">Postcode</label>
                    <input type="search" id="postcode" form="subform">`,
      profile: TEST_PROFILE,
      autofillTrigger: "#postcode",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          invalid: true,
          fields: [
            { fieldName: "address-line1" },
            { fieldName: "address-line2", reason: "update-heuristic" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
            { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
            { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          ],
        },
      ],
    },
    {
      description: "several fields with form attributes",
      fixtureData: `<form id="mainform">
                      <label for="street1">Address line 1</label>
                      <input type="text" id="street1">
                      <label for="email">Email</label>
                      <input type="text" id="email" form="subform">
                    </form>
                    <form id="subform">
                      <label for="firstname">First Name</label>
                      <input type="text" id="firstname" form="subform">
                      <label for="lastname">Last Name</label>
                      <input type="text" id="lastname" form="mainform">
                      <label for="postcode">Postcode</label>
                      <input type="search" id="postcode">
                    </form>`,
      profile: TEST_PROFILE,
      autofillTrigger: "#postcode",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          invalid: true,
          fields: [
            { fieldName: "address-line1" },
            { fieldName: "family-name" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "email", autofill: TEST_PROFILE.email },
            { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
            { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          ],
        },
      ],
    },
  ],
  "fixtures/"
);

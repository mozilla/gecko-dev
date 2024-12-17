/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE = {
  "given-name": "Timothy",
  "additional-name": "John",
  "family-name": "Berners-Lee",
  organization: "Mozilla",
  "street-address": "331 E Evelyn Ave",
  "address-level2": "Mountain View",
  "address-level1": "CA",
  "postal-code": "94041",
  country: "US",
  tel: "+16509030800",
  email: "address@mozilla.org",
};

add_autofill_heuristic_tests([
  {
    fixtureData: `
        <html><body>
          <input id="email" autocomplete="email">
          <input id="postal-code" autocomplete="postal-code">
          <input id="country" autocomplete="country">
        </body></html>
      `,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          { fieldName: "country", autofill: TEST_PROFILE.country },
        ],
      },
    ],
  },
  {
    description: "autofill multiple email fields(2)",
    fixtureData: `
        <html><body>
          <input id="email" autocomplete="email">
          <input id="email" autocomplete="email">
          <input id="postal-code" autocomplete="postal-code">
          <input id="country" autocomplete="country">
        </body></html>
      `,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          { fieldName: "country", autofill: TEST_PROFILE.country },
        ],
      },
    ],
  },
  {
    description: "autofill multiple email fields(3)",
    fixtureData: `
        <html><body>
          <input id="email" autocomplete="email">
          <input id="email" autocomplete="email">
          <input id="email" autocomplete="email">
          <input id="postal-code" autocomplete="postal-code">
          <input id="country" autocomplete="country">
        </body></html>
      `,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          { fieldName: "country", autofill: TEST_PROFILE.country },
        ],
      },
    ],
  },
  {
    description:
      "two fields with identical field names with duplicated offscreen field",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address"></p>
          <p>Country: <input id="country-b"></p>
          <p style="position: absolute; left: -9999px;">Country: <input id="country-c"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
        </form></body></html>
      `,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          {
            fieldName: "street-address",
            autofill: TEST_PROFILE["street-address"],
          },
          { fieldName: "country", autofill: TEST_PROFILE.country },
          { fieldName: "country", autofill: TEST_PROFILE.country },
          {
            fieldName: "postal-code",
            reason: "autocomplete",
            autofill: TEST_PROFILE["postal-code"],
          },
        ],
      },
    ],
  },
]);

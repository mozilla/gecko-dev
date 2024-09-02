/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE_US = {
  email: "address_us@mozilla.org",
  organization: "Mozilla",
  "address-level1": "AZ",
  country: "US",
};

const TEST_PROFILE_CA = {
  email: "address_ca@mozilla.org",
  organization: "Mozilla",
  country: "CA",
};

const MARKUP_SELECT_COUNTRY = `
  <html><body>
    <input id="email" autocomplete="email">
    <input id="organization" autocomplete="organization">
    <select id="country" autocomplete="country">
      <option value="">Select a country</option>
      <option value="Germany">Germany</option>
      <option value="Canada">Canada</option>
      <option value="United States">United States</option>
      <option value="France">France</option>
    </select>
  </body></html>
`;

// Strip any attributes that could help identify select as country field
const MARKUP_SELECT_COUNTRY_WITHOUT_AUTOCOMPLETE = `
  <html><body>
    <input id="email">
    <input id="organization">
    <select id="country">
      <option value="">Select a country</option>
      <option value="Canada">Canada</option>
      <option value="United States">United States</option>
    </select>
  </body></html>
`;

add_autofill_heuristic_tests([
  {
    description: "Test autofill select with US profile",
    fixtureData: MARKUP_SELECT_COUNTRY,
    profile: TEST_PROFILE_US,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_US.email },
          { fieldName: "organization", autofill: TEST_PROFILE_US.organization },
          { fieldName: "country", autofill: "United States" },
        ],
      },
    ],
  },
  {
    description: "Test autofill select with CA profile",
    fixtureData: MARKUP_SELECT_COUNTRY,
    profile: TEST_PROFILE_CA,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_CA.email },
          { fieldName: "organization", autofill: TEST_PROFILE_CA.organization },
          { fieldName: "country", autofill: "Canada" },
        ],
      },
    ],
  },
  {
    description: "Test autofill <select> without autocomplete attribute",
    fixtureData: MARKUP_SELECT_COUNTRY_WITHOUT_AUTOCOMPLETE,
    profile: TEST_PROFILE_CA,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_CA.email },
          { fieldName: "organization", autofill: TEST_PROFILE_CA.organization },
          { fieldName: "country", autofill: "Canada" },
        ],
      },
    ],
  },
  {
    description:
      "Address form without matching options in select for address-level1 and country",
    fixtureData: `<form>
      <input id="email" autocomplete="email">
      <select id="address-level1" autocomplete="address-level1">
        <option id=default value=""></option>
        <option id="option-address-level1-dummy1" value="Dummy">Dummy</option>
        <option id="option-address-level1-dummy2" value="Dummy 2">Dummy 2</option>
      </select>
      <select id="country" autocomplete="country">
        <option id=default value=""></option>
        <option id="option-country-dummy1" value="Dummy">Dummy</option>
        <option id="option-country-dummy2" value="Dummy 2">Dummy 2</option>
      </select>
    </form>`,

    profile: TEST_PROFILE_US,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_US.email },
          { fieldName: "address-level1", autofill: "" },
          { fieldName: "country", autofill: "" },
        ],
      },
    ],
  },
]);

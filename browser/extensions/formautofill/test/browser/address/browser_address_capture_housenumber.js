"use strict";

// This test verifies that the address fillin popup appears correctly when
// a house number field exists and that the house number is prepended/appended
// to the street name.

let CAPTURE_FILL_VALUE = {
  "#given-name": "John",
  "#family-name": "Doe",
  "#street-address": "Vassar Street",
  "#address-housenumber": "32",
  "#address-level1": "MA",
  "#address-level2": "Cambridge",
  "#postal-code": "02139",
};

let CAPTURE_EXPECTED_RECORD_US = {
  name: "John Doe",
  "street-address": "32 Vassar Street",
  "address-level1": "MA",
  "address-level2": "Cambridge",
  "postal-code": "02139",
  country: "US",
};

let CAPTURE_EXPECTED_RECORD_DE = {
  name: "John Doe",
  "street-address": "Vassar Street 32",
  "address-level2": "Cambridge",
  "postal-code": "02139",
  country: "DE",
};

add_capture_heuristic_tests([
  {
    description: `House number captured US`,
    region: "US",
    fixtureData: `
      <form id="form">
        <p><label>givenname: <input type="text" id="given-name" autocomplete="given-name" /></label></p>
        <p><label>familyname: <input type="text" id="family-name" autocomplete="family-name" /></label></p>
        <p><label>streetAddress: <input type="text" id="street-address" autocomplete="street-address" /></label></p>
        <p><label>houseNumber: <input type="text" id="address-housenumber" /></label></p>
        <p><label>addressLevel2: <input type="text" id="address-level2" autocomplete="address-level2" /></label></p>
        <p><label>addressLevel1: <input type="text" id="address-level1" autocomplete="address-level1" /></label></p>
        <p><label>postalCode: <input type="text" id="postal-code" autocomplete="postal-code" /></label></p>
        <input id="submit" type="submit"/>
      </form>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "address-housenumber", reason: "regex-heuristic" },
          { fieldName: "address-level2" },
          { fieldName: "address-level1" },
          { fieldName: "postal-code" },
        ],
      },
    ],
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD_US,
  },
  {
    description: `House number captured DE`,
    region: "DE",
    fixtureData: `
      <form id="form">
        <p><label>givenname: <input type="text" id="given-name" autocomplete="given-name" /></label></p>
        <p><label>familyname: <input type="text" id="family-name" autocomplete="family-name" /></label></p>
        <p><label>streetAddress: <input type="text" id="street-address" autocomplete="street-address" /></label></p>
        <p><label>houseNumber: <input type="text" id="address-housenumber" autocomplete="house-number" /></label></p>
        <p><label>addressLevel2: <input type="text" id="address-level2" autocomplete="address-level2" /></label></p>
        <p><label>addressLevel1: <input type="text" id="address-level1" autocomplete="address-level1" /></label></p>
        <p><label>postalCode: <input type="text" id="postal-code" autocomplete="postal-code" /></label></p>
        <input id="submit" type="submit"/>
      </form>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "address-housenumber", reason: "regex-heuristic" },
          { fieldName: "address-level2" },
          { fieldName: "address-level1" },
          { fieldName: "postal-code" },
        ],
      },
    ],
    captureFillValue: CAPTURE_FILL_VALUE,
    captureExpectedRecord: CAPTURE_EXPECTED_RECORD_DE,
  },
]);

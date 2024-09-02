/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test verifies autofill on select dropdowns when one is hidden.

const TEST_PROFILE = {
  "given-name": "Joe",
  "family-name": "Smith",
  "street-address": "7 First St",
  "address-level2": "Faketown",
  "address-level1": "Kansas",
  country: "US",
};

const MARKUP_SELECT_STATE = `
  <html><body>
    First Name: <input id="fname">
    Last Name: <input id="lname">
    Address: <input id="address">
    <div id="unusedArea" style="display: none;">
      Country: <select id="unusedCountry" autocomplete="country">
          <option value="">Select a country</option>
          <option value="Australia">Australia</option>
          <option value="Indonesia">Indonesia</option>
          <option value="Japan">Japan</option>
          <option value="Malaysia">Malaysia</option>
          <option value="US">United States</option>
        </select>
    </div>
    <p>City: <input id="city"></p>
    <select id="country" autocomplete="country">
      <option value="">Select a country</option>
      <option value="Germany">Germany</option>
      <option value="Canada">Canada</option>
      <option value="United States">United States</option>
      <option value="France">France</option>
    </select>
  </body></html>
`;

const MARKUP_SELECT_STATE_WITH_AIRAHIDDEN = `
  <html><body><form>
    First Name: <input id="fname">
    Last Name: <input id="lname">
    Address: <input id="address">
    <select id="county" autocomplete="address-level1" aria-hidden="true">
      <option value="--">Select a county</option>
      <option value="C1">Cheshire</option>
      <option value="K1">Kansas</option>
      <option value="N1">Norfolk</option>
      <option value="S1">Surrey</option>
    </select>
    <select id="state" autocomplete="address-level1">
      <option value="None">Select a state</option>
      <option value="CO">Colorado</option>
      <option value="KS">Kansas</option>
      <option value="MN">Minnesota</option>
      <option value="MO">Missouri</option>
    </select>
  </form></body></html>
`;

const MARKUP_SELECT_STATE_WITH_AIRAHIDDEN_PARTIAL = `
  <html><body><form>
    First Name: <input id="fname">
    Last Name: <input id="lname">
    Address: <input id="address">
    <select id="county" autocomplete="address-level1" aria-hidden="true">
      <option value="--">Select a county</option>
      <option value="C1">Cheshire</option>
      <option value="K1">Kansas</option>
      <option value="N1">Norfolk</option>
      <option value="S1">Surrey</option>
    </select>
  </form></body></html>
`;

/* global add_heuristic_tests */

add_heuristic_tests(
  [
    {
      description:
        "When two dropdown fields exist and one is hidden, the autofill " +
        "should be performed on the visible dropdown.",
      fixtureData: MARKUP_SELECT_STATE,
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
            { fieldName: "country", autofill: "", reason: "autocomplete" },
            {
              fieldName: "address-level2",
              autofill: TEST_PROFILE["address-level2"],
            },
            {
              fieldName: "country",
              autofill: "United States",
              reason: "autocomplete",
            },
          ],
        },
      ],
    },
    {
      description:
        "When a dropdown has aria-hidden set to true, autofill should happen on the visible dropdown ",
      fixtureData: MARKUP_SELECT_STATE_WITH_AIRAHIDDEN,
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
            {
              fieldName: "address-level1",
              reason: "autocomplete",
            },
            {
              fieldName: "address-level1",
              autofill: "KS",
              reason: "autocomplete",
            },
          ],
        },
      ],
    },
    {
      description:
        "When only one dropdown exists and has aria-hidden, it will be filled in anyway",
      fixtureData: MARKUP_SELECT_STATE_WITH_AIRAHIDDEN_PARTIAL,
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
            {
              fieldName: "address-level1",
              autofill: "K1",
              reason: "autocomplete",
            },
          ],
        },
      ],
    },
  ],
  "",
  { testAutofill: true }
);

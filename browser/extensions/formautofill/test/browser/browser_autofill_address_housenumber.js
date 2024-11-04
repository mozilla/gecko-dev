/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE_CA = {
  email: "address_ca@mozilla.org",
  organization: "Mozilla",
  country: "CA",
  "street-address": "160 Main St\nApartment 306",
  "postal-code": "M5V 1R9",
};

const TEST_PROFILE_DE = {
  email: "address_de@mozilla.org",
  organization: "Mozilla",
  country: "DE",
  "street-address": "Schlesische Str 999",
};

const TEST_PROFILE_DE2 = {
  email: "address_de@mozilla.org",
  organization: "Mozilla",
  country: "DE",
  "address-level2": "Berlin",
  "street-address": "Schlesische Str 999\nApt 216",
  "postal-code": "90002",
};

add_autofill_heuristic_tests([
  {
    description: "Test autofill with house number",
    fixtureData: `<form>
      <input id="email">
      <input id="street">
      <input id="housenumber">
      <input id="unit">
    </form>`,
    profile: TEST_PROFILE_CA,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_CA.email },
          {
            fieldName: "address-line1",
            autofill: "Main St",
            reason: "update-heuristic",
          },
          { fieldName: "address-housenumber", autofill: "160" },
          { fieldName: "address-line2", autofill: "Apartment 306" },
        ],
      },
    ],
  },
  {
    description: "Test autofill with house number reversed",
    fixtureData: `<form>
      <input id="email">
      <input id="housenumber">
      <input id="street">
      <input id="unit">
    </form>`,
    profile: TEST_PROFILE_CA,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_CA.email },
          { fieldName: "address-housenumber", autofill: "160" },
          {
            fieldName: "address-line1",
            autofill: "Main St",
            reason: "update-heuristic",
          },
          { fieldName: "address-line2", autofill: "Apartment 306" },
        ],
      },
    ],
  },
  {
    description: "Test autofill with only house number and street",
    fixtureData: `<form>
      <input id="email">
      <input id="housenumber">
      <input id="street">
      <input id="postalcode">
    </form>`,
    profile: TEST_PROFILE_CA,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_CA.email },
          { fieldName: "address-housenumber", autofill: "160" },
          {
            fieldName: "street-address",
            autofill: "Main St",
          },
          { fieldName: "postal-code", autofill: "M5V 1R9" },
        ],
      },
    ],
  },
  {
    description: "Test autofill with german house number",
    fixtureData: `<form>
      <input id="organization">
      <input id="strasse">
      <input id="haus">
      <input id="adresszusatz">
    </form>`,
    profile: TEST_PROFILE_DE,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "organization", autofill: TEST_PROFILE_DE.organization },
          { fieldName: "address-line1", autofill: "Schlesische Str" },
          { fieldName: "address-housenumber", autofill: "999" },
          { fieldName: "address-line2", autofill: "" },
        ],
      },
    ],
  },
  {
    description: "Test autofill with street and house number as one field",
    fixtureData: `<form>
      <input id="email">
      <input id="street-housenumber">
      <input id="unit">
    </form>`,
    profile: TEST_PROFILE_CA,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_CA.email },
          {
            fieldName: "address-line1",
            autofill: "160 Main St",
          },
          { fieldName: "address-line2", autofill: "Apartment 306" },
        ],
      },
    ],
  },
  {
    description:
      "Test autofill with german street and house number as one field",
    fixtureData: `<form>
      <input id="email">
      <input id="strasse_haus">
      <input id="unit">
    </form>`,
    profile: TEST_PROFILE_DE,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_DE.email },
          {
            fieldName: "address-line1",
            autofill: "Schlesische Str 999",
          },
          { fieldName: "address-line2", autofill: "" },
        ],
      },
    ],
  },
  {
    description:
      "Test autofill with street and house number with address2 non-adjacent",
    fixtureData: `<form>
      <input id="strasse">
      <input id="haus">
      <input id="organization">
      <input id="city">
      <input id="address2">
      <input id="postal-code">
    </form>`,
    profile: TEST_PROFILE_DE2,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "address-line1", autofill: "Schlesische Str" },
          { fieldName: "address-housenumber", autofill: "999" },
          {
            fieldName: "organization",
            autofill: TEST_PROFILE_DE2.organization,
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_DE2["address-level2"],
          },
          { fieldName: "address-line2", autofill: "Apt 216" },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_DE2["postal-code"],
          },
        ],
      },
    ],
  },
]);

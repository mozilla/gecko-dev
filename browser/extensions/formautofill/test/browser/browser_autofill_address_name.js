/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE = {
  "given-name": "John",
  "additional-name": "Middle",
  "family-name": "Doe",
  organization: "Mozilla",
  email: "john.doe@mozilla.com",
  country: "US",
  "street-address": "123 Sesame Street",
};

add_autofill_heuristic_tests([
  {
    description: "Test consecutive family-name/given-name input pairs",
    fixtureData: `<form>
      <input id="family-name-1"  autocomplete="family-name">
      <input id="given-name-1" autocomplete="given-name">
      <input id="family-name-2"  autocomplete="family-name">
      <input id="given-name-2"  autocomplete="given-name">
      <input id="email"  autocomplete="email">
      <input id="organization"  autocomplete="organization">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "organization", autofill: TEST_PROFILE.organization },
        ],
      },
    ],
  },
  {
    description:
      "Test consecutive given-name/additional-name/family-name input pairs",
    fixtureData: `<form>
      <input id="given-name-1" autocomplete="given-name">
      <input id="additional-name-1"  autocomplete="additional-name">
      <input id="family-name-1"  autocomplete="family-name">
      <input id="given-name-2"  autocomplete="given-name">
      <input id="additional-name-2"  autocomplete="additional-name">
      <input id="family-name-2"  autocomplete="family-name">
      <input id="email"  autocomplete="email">
      <input id="organization"  autocomplete="organization">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          {
            fieldName: "additional-name",
            autofill: TEST_PROFILE["additional-name"],
          },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          {
            fieldName: "additional-name",
            autofill: TEST_PROFILE["additional-name"],
          },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "organization", autofill: TEST_PROFILE.organization },
        ],
      },
    ],
  },
  {
    description: "Test non-consecutive family-name/given-name input pairs",
    fixtureData: `<form>
      <input id="email"  autocomplete="email">
      <input id="given-name-1" autocomplete="given-name">
      <input id="family-name-1"  autocomplete="family-name">
      <input id="organization"  autocomplete="organization">
      <input id="given-name-2"  autocomplete="given-name">
      <input id="family-name-2"  autocomplete="family-name">
      <input id="country"  autocomplete="country">
      <input id="street-address"  autocomplete="street-address">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          { fieldName: "organization", autofill: TEST_PROFILE.organization },
        ],
      },
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          { fieldName: "country", autofill: TEST_PROFILE.country },
          {
            fieldName: "street-address",
            autofill: TEST_PROFILE["street-address"],
          },
        ],
      },
    ],
  },
]);

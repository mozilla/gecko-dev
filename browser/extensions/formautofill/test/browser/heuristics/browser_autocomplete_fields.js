/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global add_heuristic_tests */

add_heuristic_tests([
  {
    description: "Form containing 8 fields with autocomplete attribute.",
    fixtureData: `<form>
                   <input id="given-name" autocomplete="given-name">
                   <input id="additional-name" autocomplete="additional-name">
                   <input id="family-name" autocomplete="family-name">
                   <input id="street-address" autocomplete="street-address">
                   <input id="city" autocomplete="address-level2">
                   <input id="country" autocomplete="country">
                   <input id="email" autocomplete="email">
                   <input id="tel" autocomplete="tel">
                   <input id="without-autocomplete-1">
                   <input id="without-autocomplete-2">
                 </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "additional-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "address-level2" },
          { fieldName: "country" },
          { fieldName: "email" },
          { fieldName: "tel" },
        ],
      },
    ],
  },
  {
    description: "Form containing only 2 fields with autocomplete attribute.",
    fixtureData: `<form>
                   <input id="street-address" autocomplete="street-address">
                   <input id="city" autocomplete="address-level2">
                   <input id="without-autocomplete-1">
                   <input id="without-autocomplete-2">
                 </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        invalid: true,
        fields: [
          { fieldName: "street-address" },
          { fieldName: "address-level2" },
        ],
      },
    ],
  },
  {
    description: "Form containing credit card autocomplete attributes.",
    fixtureData: `<form>
                  <input id="cc-number" autocomplete="cc-number">
                  <input id="cc-name" autocomplete="cc-name">
                  <input id="cc-exp-month" autocomplete="cc-exp-month">
                  <input id="cc-exp-year" autocomplete="cc-exp-year">
                 </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp-month" },
          { fieldName: "cc-exp-year" },
        ],
      },
    ],
  },
]);

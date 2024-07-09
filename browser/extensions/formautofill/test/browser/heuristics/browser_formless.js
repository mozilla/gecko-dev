/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global add_heuristic_tests */

add_heuristic_tests([
  {
    description: "Fields without form element.",
    fixtureData: `
        <input id="street-address" autocomplete="street-address">
       <input id="city" autocomplete="address-level2">
       <input id="country" autocomplete="country">
       <input id="email" autocomplete="email">
       <input id="tel" autocomplete="tel">
       <input id="without-autocomplete-1">
       <input id="without-autocomplete-2">`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "address-level2" },
          { fieldName: "country" },
          { fieldName: "email" },
          { fieldName: "tel" },
        ],
      },
    ],
  },
]);

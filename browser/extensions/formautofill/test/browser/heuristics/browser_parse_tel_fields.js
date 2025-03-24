/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description:
      "Address form with tel-country-code select element (Bug 1951890).",
    fixtureData: `
          <form>
            <input type="text" id="name" autocomplete="name"/>
            <input type="text" id="country" autocomplete="country"/>
            <input type="text" id="street-address" autocomplete="street-address"/>
            <input type="text" id="address-line1" autocomplete="address-line1"/>
            <input type="tel" id="tel" autocomplete="tel"/>
            <select name="phone_country_select">
          </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "name" },
          { fieldName: "country" },
          { fieldName: "street-address" },
          { fieldName: "address-line1" },
          { fieldName: "tel" },
          { fieldName: "tel-country-code", reason: "regex-heuristic" },
        ],
      },
    ],
  },
]);

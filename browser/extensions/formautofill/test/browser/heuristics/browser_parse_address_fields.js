/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description:
      "When a <select> is next to address-level2, we assume the select field is address-level1",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="country" autocomplete="country"/>
            <select><option value="test">test</option></select>
            <input type="text" id="address-level2" autocomplete="address-level2" />
          </form>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="country" autocomplete="country"/>
            <input type="text" id="address-level2" autocomplete="address-level2" />
            <select><option value="test">test</option></select>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "name" },
          { fieldName: "country" },
          { fieldName: "address-level1", reason: "update-heuristic" },
          { fieldName: "address-level2" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "name" },
          { fieldName: "country" },
          { fieldName: "address-level2" },
          { fieldName: "address-level1", reason: "update-heuristic" },
        ],
      },
    ],
  },
  {
    description:
      "When a label contains both organization-related keywords and all other address-related keywords, it should be an address",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="country" autocomplete="country"/>
            <input type="text" id="address-line1" autocomplete="address-line1"/>
            <label for="organization-address-line2">organization-address-2</label>
            <input type="text" id="organization-address-line2" />
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "name" },
          { fieldName: "country" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2", reason: "update-heuristic" },
        ],
      },
    ],
  },
]);

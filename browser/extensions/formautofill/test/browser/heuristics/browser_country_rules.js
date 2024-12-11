/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: "Land matches country",
    fixtureData: `
        <html>
        <body>
          <form>
            <input id="strasse"/>
            <input id="postal"/>
            <select id="land"/>
              <option value="fr">Frankreich</option>
              <option value="de">Deutschland</option>
              <option value="ca">Kanada</option>
              <option value="us">USA</option>
            </select>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "address-line1" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: "Landmark matches address-line2",
    fixtureData: `
        <html>
        <body>
          <form>
            <input id="strasse"/>
            <input id="landmark"/>
            <input id="postal"/>
            <select id="land"/>
              <option value="fr">Frankreich</option>
              <option value="de">Deutschland</option>
              <option value="ca">Kanada</option>
              <option value="us">USA</option>
            </select>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "address-line1" },
          { fieldName: "address-line2" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
]);

/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: "Two fields with identical field names",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address-b" autocomplete="address-line1"></p>
          <p>Organization: <input id="organization-b" autocomplete="address-line1"></p>
          <p>City: <input id="city-b"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "address-line1", reason: "autocomplete" },
          { fieldName: "address-line1", reason: "autocomplete" },
          { fieldName: "address-level2" },
          { fieldName: "postal-code", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description:
      "Two fields with identical field names with a fixed section name",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address-b" autocomplete="shipping address-line1"></p>
          <p>Organization: <input id="organization-b" autocomplete="shipping address-line1"></p>
          <p>City: <input id="city-b"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            addressType: "shipping",
          },
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            addressType: "shipping",
          },
          { fieldName: "address-level2" },
          { fieldName: "postal-code", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description:
      "Two fields with identical field names with different section names",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address-b" autocomplete="shipping address-line1"></p>
          <p>Organization: <input id="organization-b" autocomplete="billing address-line1"></p>
          <p>City: <input id="city-b"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            addressType: "shipping",
          },
        ],
      },
      {
        fields: [
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            addressType: "billing",
          },
          { fieldName: "address-level2", reason: "regex-heuristic" },
          { fieldName: "postal-code", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description:
      "Two fields with identical field names with different explicit section names on all fields",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b" autocomplete="shipping given-name"></p>
          <p>Last Name: <input id="lname-b" autocomplete="shipping family-name"></p>
          <p>Address: <input id="address-b" autocomplete="shipping address-line1"></p>
          <p>Organization: <input id="organization-b" autocomplete="billing address-line1"></p>
          <p>City: <input id="city-b" autocomplete="shipping address-level2"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="shipping postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "shipping",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "address-line1" },
          { fieldName: "address-level2" },
          { fieldName: "postal-code" },
        ],
      },
      {
        invalid: true,
        fields: [
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            addressType: "billing",
          },
        ],
      },
    ],
  },
  {
    description:
      "Two fields with identical field names with different named sections",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b" autocomplete="section-one given-name"></p>
          <p>Last Name: <input id="lname-b" autocomplete="section-one family-name"></p>
          <p>Address: <input id="address-b" autocomplete="section-one address-line1"></p>
          <p>Organization: <input id="organization-b" autocomplete="section-two address-line1"></p>
          <p>City: <input id="city-b" autocomplete="section-one address-level2"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="section-one postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          section: "section-one",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "address-line1" },
          { fieldName: "address-level2" },
          { fieldName: "postal-code" },
        ],
      },
      {
        invalid: true,
        fields: [
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            section: "section-two",
          },
        ],
      },
    ],
  },
  {
    description: "Two duplicated fields",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
          <p>Country: <input id="country-b"></p>
          <p style="position: absolute; left: -9999px;">Country: <input id="country-c"></p>
          <p style="position: absolute; left: -9999px;">Postal Code: <input id="postalcode-c" autocomplete="postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "country" },
          { fieldName: "country" },
        ],
      },
      {
        invalid: true,
        fields: [{ fieldName: "postal-code", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: "Three duplicated fields",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
          <p>Country: <input id="country-b"></p>
          <p style="position: absolute; left: -9999px;">Country: <input id="country-c"></p>
          <p style="position: absolute; left: -9999px;">Postal Code: <input id="postalcode-c" autocomplete="postal-code"></p>
          <p>Address: <input id="address-b" autocomplete="address-line1"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "country" },
          { fieldName: "country" },
        ],
      },
      {
        invalid: true,
        fields: [
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "address-line1", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description: "Three duplicated fields not in a row",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
          <p style="position: absolute; left: -9999px;">Country: <input id="country-c"></p>
          <p>Country: <input id="country-b"></p>
          <p style="position: absolute; left: -9999px;">Postal Code: <input id="postalcode-c" autocomplete="postal-code"></p>
          <p>Email: <input id="email"></p>
          <p>Address: <input id="address-b" autocomplete="address-line1"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "country" },
          { fieldName: "country" },
        ],
      },
      {
        fields: [
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "email", reason: "regex-heuristic" },
          { fieldName: "address-line1", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description: "Four duplicated fields",
    fixtureData: `
        <html><body><form>
          <p>Country: <input id="country-a" autocomplete="shipping country"></p>
          <p>Country: <input id="country-b" autocomplete="billing country"></p>
          <p>First Name: <input id="given-name-c" autocomplete="shipping given-name"></p>
          <p>Country: <input id="country-c" autocomplete="billing country"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        invalid: true,
        fields: [
          { fieldName: "country", addressType: "shipping" },
          { fieldName: "given-name", addressType: "shipping" },
        ],
      },
      {
        invalid: true,
        fields: [
          { fieldName: "country", addressType: "billing" },
          { fieldName: "country", addressType: "billing" },
        ],
      },
    ],
  },
  {
    description: "Five duplicated fields",
    fixtureData: `
        <html><body><form>
          <p>Country: <input id="country-a" autocomplete="shipping country"></p>
          <p>Country: <input id="country-b" autocomplete="billing country"></p>
          <p>First Name: <input id="given-name-c" autocomplete="shipping given-name"></p>
          <p>Country: <input id="country-c" autocomplete="billing country"></p>
          <p>Country: <input id="country-d" autocomplete="billing country"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        invalid: true,
        fields: [
          { fieldName: "country", addressType: "shipping" },
          { fieldName: "given-name", addressType: "shipping" },
        ],
      },
      {
        invalid: true,
        fields: [
          { fieldName: "country", addressType: "billing" },
          { fieldName: "country", addressType: "billing" },
          { fieldName: "country", addressType: "billing" },
        ],
      },
    ],
  },
  {
    description: "Four duplicated fields with postal code",
    fixtureData: `
        <html><body><form>
          <p>Postal Code: <input id="postalcode-a" autocomplete="shipping postal-code"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="billing postal-code"></p>
          <p>Telephone: <input id="tel-c" autocomplete="shipping tel"></p>
          <p>Postal Code: <input id="postalcode-c" autocomplete="billing postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        invalid: true,
        fields: [
          { fieldName: "postal-code", addressType: "shipping" },
          { fieldName: "tel", addressType: "shipping" },
        ],
      },
      {
        invalid: true,
        fields: [
          { fieldName: "postal-code", addressType: "billing" },
          { fieldName: "postal-code", addressType: "billing" },
        ],
      },
    ],
  },
  {
    description: "Five duplicated fields with postal code",
    fixtureData: `
        <html><body><form>
          <p>Postal Code: <input id="postalcode-a" autocomplete="shipping postal-code"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="billing postal-code"></p>
          <p>Telephone: <input id="tel-c" autocomplete="shipping tel"></p>
          <p>Postal Code: <input id="postalcode-c" autocomplete="billing postal-code"></p>
          <p>Postal Code: <input id="postalcode-d" autocomplete="billing postal-code"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        invalid: true,
        fields: [
          { fieldName: "postal-code", addressType: "shipping" },
          { fieldName: "tel", addressType: "shipping" },
        ],
      },
      {
        invalid: true,
        fields: [
          { fieldName: "postal-code", addressType: "billing" },
          { fieldName: "postal-code", addressType: "billing" },
          { fieldName: "postal-code", addressType: "billing" },
        ],
      },
    ],
  },
  {
    description: "Several of the same type of field duplicated",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
          <p style="position: absolute; left: -9999px;">Country: <input id="country-c"></p>
          <p>Country: <input id="country-b"></p>
          <p style="position: absolute; left: -9999px;">Postal Code: <input id="postalcode-c" autocomplete="postal-code"></p>
          <p>Email: <input id="email"></p>
          <p>Birth Country: <input id="country-d"></p>
          <p>Other Country: <input id="country-e"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "country" },
          { fieldName: "country" },
        ],
      },
      {
        fields: [
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "email", reason: "regex-heuristic" },
          { fieldName: "country", reason: "regex-heuristic" },
          { fieldName: "country", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description: "Three duplicated fields not in a row with sections",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address" autocomplete="billing address-line1"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="shipping postal-code"></p>
          <p style="position: absolute; left: -9999px;">Country: <input id="country-c"></p>
          <p>Country: <input id="country-b"></p>
          <p style="position: absolute; left: -9999px;">Postal Code: <input id="postalcode-c" autocomplete="shipping postal-code"></p>
          <p>Email: <input id="email"></p>
          <p>Address: <input id="address-b" autocomplete="billing address-line1"></p>
          <p>Address: <input id="address-c" autocomplete="billing address-line2"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            addressType: "billing",
          },
          {
            fieldName: "address-line1",
            reason: "autocomplete",
            addressType: "billing",
          },
          {
            fieldName: "address-line2",
            reason: "autocomplete",
            addressType: "billing",
          },
        ],
      },
      {
        invalid: true,
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "postal-code",
            addressType: "shipping",
            reason: "autocomplete",
          },
          { fieldName: "country" },
          { fieldName: "country" },
        ],
      },
      {
        invalid: true,
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "postal-code",
            reason: "autocomplete",
            addressType: "shipping",
          },
          { fieldName: "email" },
        ],
      },
    ],
  },
  {
    description: "Many duplicated fields",
    fixtureData: `
        <html><body><form>
          <p>First Name: <input id="fname-s"></p>
          <p>Last Name: <input id="lname-s"></p>
          <p>Address: <input id="address" autocomplete="address-line1"></p>
          <p>Postal Code: <input id="postalcode-s" autocomplete="postal-code"></p>
          <p>Country: <input id="country-s"></p>
          <p>First Name: <input id="fname-b"></p>
          <p>Last Name: <input id="lname-b"></p>
          <p>Address: <input id="address-b" autocomplete="address-line1"></p>
          <p>Postal Code: <input id="postalcode-b" autocomplete="postal-code"></p>
          <p>Country: <input id="country-b"></p>
        </form></body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "address-line1", reason: "autocomplete" },
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "country" },
        ],
      },
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "address-line1", reason: "autocomplete" },
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: `Form field with hidden attribute`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="email"/>
            <input type="text" autocomplete="tel" hidden/>
            <input type="text" autocomplete="email"/>
        </body></html>
      `,
    expectedResult: [
      {
        invalid: true,
        default: {
          reason: "autocomplete",
        },
        fields: [{ fieldName: "email" }, { fieldName: "email" }],
      },
    ],
  },
  {
    description: `Form field with hidden attribute`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="shipping email"/>
            <input type="text" autocomplete="shipping tel" hidden/>
            <input type="text" autocomplete="billing email"/>
            <input type="text" autocomplete="billing email" hidden/>
            <input type="text" autocomplete="postal-code"/>
        </body></html>
      `,
    expectedResult: [
      {
        invalid: true,
        default: {
          reason: "autocomplete",
          addressType: "shipping",
        },
        fields: [{ fieldName: "email" }],
      },
      {
        invalid: true,
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", addressType: "billing" },
          { fieldName: "postal-code" },
        ],
      },
    ],
  },
]);

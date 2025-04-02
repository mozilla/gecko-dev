/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: "Apply heuristic when we only see one street-address fields",
    fixtureData: `
        <html><body>
          <form><input type="text" id="street-address"/></form>
          <form><input type="text" id="addr-1"/></form>
          <form><input type="text" id="addr-2"/></form>
          <form><input type="text" id="addr-3"/></form>
        </body></html>`,
    expectedResult: [
      {
        invalid: true,
        fields: [{ fieldName: "street-address", reason: "regex-heuristic" }],
      },
      {
        invalid: true,
        fields: [{ fieldName: "address-line1", reason: "regex-heuristic" }],
      },
      {
        invalid: true,
        fields: [{ fieldName: "address-line1", reason: "update-heuristic" }],
      },
      {
        invalid: true,
        fields: [{ fieldName: "address-line1", reason: "update-heuristic" }],
      },
    ],
  },
  {
    // Bug 1833613
    description:
      "street-address field is treated as address-line1 when address-line2 is present while adddress-line1 is not",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="street-address" autocomplete="street-address"/>
            <input type="text" id="address-line2" autocomplete="address-line2"/>
            <input type="text" id="email" autocomplete="email"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "address-line1", reason: "update-heuristic" },
          { fieldName: "address-line2", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    // Bug 1833613
    description:
      "street-address field should not be treated as address-line1 when address-line2 is not present",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="street-address" autocomplete="street-address"/>
            <input type="text" id="address-line3" autocomplete="address-line3"/>
            <input type="text" id="email" autocomplete="email"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "street-address", reason: "autocomplete" },
          { fieldName: "address-line3", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    // Bug 1833613
    description:
      "street-address field should not be treated as address-line1 when address-line1 is present",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="street-address" autocomplete="street-address"/>
            <input type="text" id="address-line1" autocomplete="address-line1"/>
            <input type="text" id="email" autocomplete="email"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "street-address", reason: "autocomplete" },
          { fieldName: "address-line1", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description:
      "street-address field is treated as address-line1 when address-line2 is present while adddress-line1 is not",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="addr-3"/>
            <input type="text" id="addr-2"/>
            <input type="text" id="addr-1"/>
          </form>
          <form>
            <input type="text" id="addr-3" autocomplete="address-line3"/>
            <input type="text" id="addr-2" autocomplete="address-line2"/>
            <input type="text" id="addr-1" autocomplete="address-line1"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        description:
          "Apply heuristic when we see 3 street-address fields occur in a row",
        fields: [
          { fieldName: "address-line1", reason: "update-heuristic" },
          { fieldName: "address-line2", reason: "regex-heuristic" },
          { fieldName: "address-line3", reason: "update-heuristic" },
        ],
      },
      {
        description:
          "Do not apply heuristic when we see 3 street-address fields occur in a row but autocomplete attribute is present",
        fields: [
          { fieldName: "address-line3", reason: "autocomplete" },
          { fieldName: "address-line2", reason: "autocomplete" },
          { fieldName: "address-line1", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description: "address field matches cc-number as well as address-line1",
    fixtureData: `
        <html><body><form>
          <label for="first-name">First Name</label>
          <input id="first-name">
          <label for="last-name">Last Name</label>
          <input id="last-name">
          <label for="a1">Saisir une adresse numero de maison inclus</label>
          <input id="a1">
        </form></body></html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "family-name", reason: "regex-heuristic" },
          { fieldName: "address-line1", reason: "update-heuristic-alternate" },
        ],
      },
    ],
  },
  {
    description: "address field matches house number",
    fixtureData: `
        <html><body><form>
          <label for="strasse">Street</label>
          <input id="strasse">
          <label for="haus">Haus</label>
          <input id="haus">
          <label for="adresszusatz"></label>
          <input id="adresszusatz">
        </form></body></html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "address-line1", reason: "update-heuristic" },
          { fieldName: "address-housenumber", reason: "regex-heuristic" },
          { fieldName: "address-line2", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description: "address1 and address2 not adjacent",
    fixtureData: `<form>
      <input id="firstname">
      <input id="lastname">
      <input id="address1">
      <input id="postalcode">
      <input id="city">
      <input id="address2">
    </form>`,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "address-line1" },
          { fieldName: "postal-code" },
          { fieldName: "address-level2" },
          { fieldName: "address-line2" },
        ],
      },
    ],
  },
  {
    description: "address1 and address2 not adjacent with house number",
    fixtureData: `<form>
      <input id="firstname">
      <input id="lastname">
      <input id="strasse">
      <input id="haus">
      <input id="organization">
      <input id="city">
      <input id="address2">
      <input id="postal-code">
    </form>`,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "address-line1" },
          { fieldName: "address-housenumber" },
          { fieldName: "organization" },
          { fieldName: "address-level2" },
          { fieldName: "address-line2" },
          { fieldName: "postal-code" },
        ],
      },
    ],
  },
  {
    description: "Form containing type='search' with various address types.",
    fixtureData: `<form>
                  <input id="firstname" autocomplete="firstname" type="search">
                  <label>Last Name: <input id="lastname"></label>
                  <label>Middle Name: <input id="middlename" type="search"></label>
                  <label>Street Address: <input id="address-line1" type="search"></label>
                  <input id="address-line2">
                  <label>City: <input id="address-level2" type="search"></label>
                  <label>Email: <input id="email" type="search"></label>
                  <input id="phone">
                  <label>Postal Code: <input id="postalcode1" type="search"></label>
                 </form>`,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "family-name" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2", reason: "update-heuristic" },
          { fieldName: "address-level2" },
          { fieldName: "tel" },
          { fieldName: "postal-code" },
        ],
      },
    ],
  },
  {
    description:
      "<input> fields with type='search' should always be recognized when the field type is determined by the autocomplete attribute",
    fixtureData: `<form>
        <label>given-name: <input type="search" autocomplete="given-name" autofocus></label>
        <label>additional-name: <input type="search" autocomplete="additional-name"></label>
        <label>family-name: <input type="search" autocomplete="family-name"></label>
        <label>name: <input type="search" autocomplete="name"></label>
        <label>organization: <input type="search" autocomplete="organization"></label>
        <label>street-address: <input type="search" autocomplete="street-address"></label>
        <label>address-line1: <input type="search" autocomplete="address-line1"></label>
        <label>address-line2: <input type="search" autocomplete="address-line2"></label>
        <label>address-line3: <input type="search" autocomplete="address-line3"></label>
        <label>address-level2: <input type="search" autocomplete="address-level2"></label>
        <label>address-level1: <input type="search" autocomplete="address-level1"></label>
        <label>postal-code: <input type="search" autocomplete="postal-code"></label>
        <label>country (abbr.): <input type="search" autocomplete="country"></label>
        <label>country-name: <input type="search" autocomplete="country-name"></label>
        <label>email: <input type="search" autocomplete="email"></label>
        <label>tel: <input type="search" autocomplete="tel"></label>
        <label>tel-country-code: <input type="search" autocomplete="tel-country-code"></label>
        <label>tel-national: <input type="search" autocomplete="tel-national"></label>
        <label>tel-area-code: <input type="search" autocomplete="tel-area-code"></label>
        <label>tel-local: <input type="search" autocomplete="tel-local"></label>
        <label>tel-local-prefix: <input type="search" autocomplete="tel-local-prefix"></label>
        <label>tel-local-suffix: <input type="search" autocomplete="tel-local-suffix"></label>
        <label>cc-name: <input type="search" autocomplete="cc-name" autofocus></label>
        <label>cc-given-name: <input type="search" autocomplete="cc-given-name"></label>
        <label>cc-additional-name: <input type="search" autocomplete="cc-additional-name"></label>
        <label>cc-family-name: <input type="search" autocomplete="cc-family-name"></label>
        <label>cc-number: <input type="search" autocomplete="cc-number"></label>
        <label>cc-exp-month: <input type="search" autocomplete="cc-exp-month"></label>
        <label>cc-exp-year: <input type="search" autocomplete="cc-exp-year"></label>
        <label>cc-exp: <input type="search" autocomplete="cc-exp"></label>
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
          { fieldName: "name" },
          { fieldName: "organization" },
          { fieldName: "street-address" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2" },
          { fieldName: "address-line3" },
          { fieldName: "address-level2" },
          { fieldName: "address-level1" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
          { fieldName: "country-name" },
          { fieldName: "email" },
          { fieldName: "tel" },
          { fieldName: "tel-country-code" },
          { fieldName: "tel-national" },
          { fieldName: "tel-area-code" },
          { fieldName: "tel-local" },
          { fieldName: "tel-local-prefix" },
          { fieldName: "tel-local-suffix" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-name" },
          { fieldName: "cc-given-name" },
          { fieldName: "cc-additional-name" },
          { fieldName: "cc-family-name" },
          { fieldName: "cc-number" },
          { fieldName: "cc-exp-month" },
          { fieldName: "cc-exp-year" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description:
      "<input> fields with type='search' should only be restricted to specific field types when the field type is determined by heuristics",
    fixtureData: `<form>
        <label>given name: <input id="givenname" type="search" autofocus></label>
        <label>additional name: <input type="search"></label>
        <label>family name: <input type="search"></label>
        <label>name: <input type="search"></label>
        <label>organization: <input type="search"></label>
        <label>address line1: <input type="search"></label>
        <label>address line2: <input type="search"></label>
        <label>address line3: <input type="search"></label>
        <label>state: <input type="search"></label>
        <label>city: <input type="search"></label>
        <label>neighbourhood: <input type="search"></label>
        <label>country: <input type="search"></label>
        <label>postal code: <input type="search"></label>
        <label>streetaddress: <input type="search"></label>
        <label>email: <input type="search"></label>
        <label>tel: <input type="search"></label>
        <label>tel-country-code: <input type="search"></label>
        <label>tel-national: <input type="search"></label>
        <label>tel-area-code: <input type="search"></label>
        <label>tel-local: <input type="search"></label>
        <label>tel-local-prefix: <input type="search"></label>
        <label>tel-local-suffix: <input type="search"></label>
        <label>cc-name: <input type="search"></label>
        <label>cc-given-name: <input type="search"></label>
        <label>cc-additional-name: <input type="search"></label>
        <label>cc-family-name: <input type="search"></label>
        <label>cc-number: <input type="search"></label>
        <label>cc-exp-month: <input type="search"></label>
        <label>cc-exp-year: <input type="search"></label>
        <label>cc-exp: <input type="search"></label>
                 </form>`,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "address-line1" },
          { fieldName: "address-line2", reason: "update-heuristic" },
          { fieldName: "address-line3", reason: "update-heuristic" },
          { fieldName: "address-level1" },
          { fieldName: "address-level2" },
          { fieldName: "postal-code" },
          { fieldName: "street-address" },
        ],
      },
    ],
  },
]);

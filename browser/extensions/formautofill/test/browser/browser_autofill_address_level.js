/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE_BR = {
  "given-name": "Carlos",
  "family-name": "Alves",
  "street-address": "160 Rua Acores\nApartment 300",
  "address-level1": "São Paulo",
  "address-level2": "Sampletown",
  "address-level3": "Somewhere",
  "postal-code": "04829-310",
};

add_autofill_heuristic_tests([
  {
    description: "Test autofill with address-level3 autocomplete",
    fixtureData: `<form>
      <input id="name"/>
      <input id="address-line1"/>
      <input id="address-line2">
      <input id="address-level1">
      <input id="address-level2">
      <input id="postcode">
      <input id="extrainfo" autocomplete="address-level3">
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "160 Rua Acores" },
          {
            fieldName: "address-line2",
            autofill: "Apartment 300",
            reason: "update-heuristic",
          },
          {
            fieldName: "address-level1",
            autofill: TEST_PROFILE_BR["address-level1"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
          {
            fieldName: "address-level3",
            autofill: TEST_PROFILE_BR["address-level3"],
            reason: "autocomplete",
          },
        ],
      },
    ],
  },
  {
    description: "Test autofill with address-level3",
    fixtureData: `<form>
      <input id="name"/>
      <input id="address-line1"/>
      <input id="address-line2">
      <input id="address-level1">
      <input id="address-level2">
      <input id="address-level3">
      <input id="postcode">
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "160 Rua Acores" },
          {
            fieldName: "address-line2",
            autofill: "Apartment 300",
            reason: "update-heuristic",
          },
          {
            fieldName: "address-level1",
            autofill: TEST_PROFILE_BR["address-level1"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
          },
          {
            fieldName: "address-level3",
            autofill: TEST_PROFILE_BR["address-level3"],
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
        ],
      },
    ],
  },
  {
    description: "Test autofill with label for neighbourhood",
    fixtureData: `<form>
      <label>Nome <input id="field1"/></label>
      <label>Endereço <input id="field2"/></label>
      <label>Apartamento<input id="field3" autocomplete="address-line2"></label>
      <label>CEP <input id="field4"></label>
      <label>Bairro <input id="field5"></label>
      <label>Cidade <input id="field6"></label>
      <label>Estado <input id="field7"></label>
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "160 Rua Acores" },
          {
            fieldName: "address-line2",
            autofill: "Apartment 300",
            reason: "autocomplete",
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
          {
            fieldName: "address-level3",
            autofill: TEST_PROFILE_BR["address-level3"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
          },
          {
            fieldName: "address-level1",
            autofill: TEST_PROFILE_BR["address-level1"],
          },
        ],
      },
    ],
  },
  {
    description: "Test autofill with label for neighbourhood and house number",
    fixtureData: `<form>
      <label>Nome <input id="field1"/></label>
      <label>Endereço <input id="field2"/></label>
      <label>Número<input id="field3"/></label>
      <label>Apartamento<input id="field4"></label>
      <label>CEP <input id="field5"></label>
      <label>Estado <input id="field6"></label>
      <label>Cidade <input id="field7"></label>
      <label>Bairro <input id="field8"></label>
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "Rua Acores" },
          {
            fieldName: "address-housenumber",
            autofill: "160",
            reason: "update-heuristic",
          },
          { fieldName: "address-line2", autofill: "Apartment 300" },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
          {
            fieldName: "address-level1",
            autofill: TEST_PROFILE_BR["address-level1"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
          },
          {
            fieldName: "address-level3",
            autofill: TEST_PROFILE_BR["address-level3"],
          },
        ],
      },
    ],
  },
  {
    description:
      "Test autofill with alternative label text (complentary and numero)",
    fixtureData: `<form>
      <label>Nome <input id="field1"/></label>
      <label>Endereço <input id="field2"/></label>
      <label>Numero<input id="field3"/></label>
      <label>Complemento<input id="field4"></label>
      <label>CEP <input id="field5"></label>
      <label>Estado <input id="field6"></label>
      <label>Cidade <input id="field7"></label>
      <label>Bairro <input id="field8"></label>
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "Rua Acores" },
          {
            fieldName: "address-housenumber",
            autofill: "160",
            reason: "update-heuristic",
          },
          { fieldName: "address-line2", autofill: "Apartment 300" },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
          {
            fieldName: "address-level1",
            autofill: TEST_PROFILE_BR["address-level1"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
          },
          {
            fieldName: "address-level3",
            autofill: TEST_PROFILE_BR["address-level3"],
          },
        ],
      },
    ],
  },
  {
    description:
      "Suburb matches address-level2 when address-level2 is not present",
    fixtureData: `<form>
      <label>Name <input id="field1"/></label>
      <label>Address Line 1 <input id="field2"/></label>
      <label>Address Line 2<input id="field3"/></label>
      <label>City / Suburb<input id="field4"></label>
      <label>Postcode<input id="field5"></label>
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "160 Rua Acores" },
          {
            fieldName: "address-line2",
            autofill: "Apartment 300",
            reason: "update-heuristic",
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
            reason: "update-heuristic",
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
        ],
      },
    ],
  },
  {
    description: "Suburb matches address-level3 when address-level2 is present",
    fixtureData: `<form>
      <label>Name <input id="field1"/></label>
      <label>Address Line 1 <input id="field2"/></label>
      <label>Address Line 2<input id="field3"/></label>
      <label>City<input id="field4"></label>
      <label>Postcode<input id="field5"></label>
      <label>Suburb<input id="field6"></label>
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "160 Rua Acores" },
          {
            fieldName: "address-line2",
            autofill: "Apartment 300",
            reason: "update-heuristic",
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
          {
            fieldName: "address-level3",
            autofill: TEST_PROFILE_BR["address-level3"],
          },
        ],
      },
    ],
  },
  {
    description:
      "Suburb matches address-level2 when another address-level2 is present but not visible",
    fixtureData: `<form>
      <label>Name <input id="field1"/></label>
      <label>Address Line 1 <input id="field2"/></label>
      <label>Address Line 2<input id="field3"/></label>
      <label>City / Suburb<input id="field4"></label>
      <label>Postcode<input id="field5"></label>
      <label style="display: none;">City<select id="field6">
        <option val="a1">Here</option>
        <option val="a2">Sampletown</option>
      </select></label>
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "160 Rua Acores" },
          {
            fieldName: "address-line2",
            autofill: "Apartment 300",
            reason: "update-heuristic",
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
            reason: "update-heuristic",
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
          {
            fieldName: "address-level2",
            autofill: "",
          },
        ],
      },
    ],
  },
  {
    description:
      "Suburb matches address-level2 when another address-level2 is present but not visible in another order",
    fixtureData: `<form>
      <label>Name <input id="field1"/></label>
      <label>Address Line 1 <input id="field2"/></label>
      <label>Address Line 2<input id="field3"/></label>
      <label style="display: none;">City<select id="field4">
        <option val="a1">Here</option>
        <option val="a2">Sampletown</option>
      </select></label>
      <label>Postcode<input id="field5"></label>
      <label>City / Suburb<select id="field6">
        <option val="a1">Here</option>
        <option val="a2">Sampletown</option>
      </select></label>
    </form>`,
    profile: TEST_PROFILE_BR,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "name",
            autofill:
              TEST_PROFILE_BR["given-name"] +
              " " +
              TEST_PROFILE_BR["family-name"],
          },
          { fieldName: "address-line1", autofill: "160 Rua Acores" },
          {
            fieldName: "address-line2",
            autofill: "Apartment 300",
            reason: "update-heuristic",
          },
          {
            fieldName: "address-level2",
            autofill: "",
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_BR["postal-code"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_BR["address-level2"],
          },
        ],
      },
    ],
  },
]);

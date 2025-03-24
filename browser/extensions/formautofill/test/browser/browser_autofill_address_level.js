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
]);

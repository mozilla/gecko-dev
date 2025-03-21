/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE_ADDRESS = {
  "given-name": "John",
  "additional-name": "R.",
  "family-name": "Smith",
  name: "John R. Smith",
  organization: "Mozilla",
  "street-address": "163 W Hastings\nSuite 209",
  "address-line1": "163 W Hastings",
  "address-line2": "Suite 209",
  "address-level3": "",
  "address-level2": "Vancouver",
  "address-level1": "BC",
  "postal-code": "V6B 1H5",
  country: "CA",
  "country-name": "Canada",
  tel: "+17787851540",
  email: "timbl@w3.org",
};

const TEST_PROFILE_CREDIT_CARD = {
  "cc-name": "John Doe",
  "cc-given-name": "John",
  "cc-family-name": "Doe",
  "cc-number": "4111111111111111",
  "cc-exp": `04/${new Date().getFullYear()}`,
  "cc-exp-month": 4,
  "cc-exp-year": new Date().getFullYear(),
};

add_autofill_heuristic_tests([
  {
    description:
      "(address)textarea should always be recognized when the field type is determined by the autocomplete attribute",
    fixtureData: `<form>
        <label>given-name: <textarea id="given-name" autocomplete="given-name" autofocus></textarea></label>
        <label>additional-name: <textarea id="additional-name"  autocomplete="additional-name"></textarea></label>
        <label>family-name: <textarea id="family-name"  autocomplete="family-name"></textarea></label>
        <label>name: <textarea id="name"  autocomplete="name"></textarea></label>
        <label>organization: <textarea id="organization"  autocomplete="organization"></textarea></label>
        <label>street-address: <textarea id="street-address"  autocomplete="street-address"></textarea></label>
        <label>address-line1: <textarea id="address-line1"  autocomplete="address-line1"></textarea></label>
        <label>address-line2: <textarea id="address-line2"  autocomplete="address-line2"></textarea></label>
        <label>address-line3: <textarea id="address-line3"  autocomplete="address-line3"></textarea></label>
        <label>address-level3: <textarea id="address-level3"  autocomplete="address-level3"></textarea></label>
        <label>address-level2: <textarea id="address-level2" autocomplete="address-level2"></textarea></label>
        <label>address-level1: <textarea id="address-level1" autocomplete="address-level1"></textarea></label>
        <label>postal-code: <textarea id="postal-code" autocomplete="postal-code"></textarea></label>
        <label>country (abbr.): <textarea id="country" autocomplete="country"></textarea></label>
        <label>country-name: <textarea id="country-name" autocomplete="country-name"></textarea></label>
        <label>email: <textarea id="email" autocomplete="email"></textarea></label>
        <label>tel: <textarea id="tel" autocomplete="tel"></textarea></label>
    </form>`,
    profile: TEST_PROFILE_ADDRESS,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          {
            fieldName: "given-name",
            autofill: TEST_PROFILE_ADDRESS["given-name"],
          },
          {
            fieldName: "additional-name",
            autofill: TEST_PROFILE_ADDRESS["additional-name"],
          },
          {
            fieldName: "family-name",
            autofill: TEST_PROFILE_ADDRESS["family-name"],
          },
          { fieldName: "name", autofill: TEST_PROFILE_ADDRESS.name },
          {
            fieldName: "organization",
            autofill: TEST_PROFILE_ADDRESS.organization,
          },
          {
            fieldName: "street-address",
            autofill: TEST_PROFILE_ADDRESS["street-address"].replace("\n", " "),
          },
          {
            fieldName: "address-line1",
            autofill: TEST_PROFILE_ADDRESS["address-line1"],
          },
          {
            fieldName: "address-line2",
            autofill: TEST_PROFILE_ADDRESS["address-line2"],
          },
          {
            fieldName: "address-line3",
            autofill: TEST_PROFILE_ADDRESS["address-line3"],
          },
          {
            fieldName: "address-level3",
            autofill: TEST_PROFILE_ADDRESS["address-level3"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE_ADDRESS["address-level2"],
          },
          {
            fieldName: "address-level1",
            autofill: TEST_PROFILE_ADDRESS["address-level1"],
          },
          {
            fieldName: "postal-code",
            autofill: TEST_PROFILE_ADDRESS["postal-code"],
          },
          { fieldName: "country", autofill: TEST_PROFILE_ADDRESS.country },
          {
            fieldName: "country-name",
            autofill: TEST_PROFILE_ADDRESS["country-name"],
          },
          { fieldName: "email", autofill: TEST_PROFILE_ADDRESS.email },
          { fieldName: "tel", autofill: TEST_PROFILE_ADDRESS.tel },
        ],
      },
    ],
  },
  {
    description:
      "(credit card)textarea should always be recognized when the field type is determined by the autocomplete attribute",
    fixtureData: `<form>
        <label>cc-name: <textarea id="cc-name" autocomplete="cc-name" autofocus></textarea></label>
        <label>cc-given-name: <textarea id="cc-given-name" autocomplete="cc-given-name"></textarea></label>
        <label>cc-additional-name: <textarea id="cc-additional-name" autocomplete="cc-additional-name"></textarea></label>
        <label>cc-family-name: <textarea id="cc-family-name" autocomplete="cc-family-name"></textarea></label>
        <label>cc-number: <textarea id="cc-number" autocomplete="cc-number"></textarea></label>
        <label>cc-exp-month: <textarea id="cc-exp-month" autocomplete="cc-exp-month"></textarea></label>
        <label>cc-exp-year: <textarea id="cc-exp-year" autocomplete="cc-exp-year"></textarea></label>
        <label>cc-exp: <textarea id="cc-exp" autocomplete="cc-exp"></textarea></label>
    </form>`,
    profile: TEST_PROFILE_CREDIT_CARD,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          {
            fieldName: "cc-name",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-name"],
          },
          {
            fieldName: "cc-given-name",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-given-name"],
          },
          {
            fieldName: "cc-additional-name",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-additional-name"],
          },
          {
            fieldName: "cc-family-name",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-family-name"],
          },
          {
            fieldName: "cc-number",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-number"],
          },
          {
            fieldName: "cc-exp-month",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-exp-month"],
          },
          {
            fieldName: "cc-exp-year",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-exp-year"],
          },
          { fieldName: "cc-exp", autofill: TEST_PROFILE_CREDIT_CARD["cc-exp"] },
        ],
      },
    ],
  },
  {
    description:
      "(address)textarea should only be restricted to specific field types when the field type is determined by heuristics",
    fixtureData: `<form>
        <label>given-name: <textarea id="given-name" placeholder="given-name" autofocus></textarea></label>
        <label>additional-name: <textarea id="additional-name"  placeholder="additional-name"></textarea></label>
        <label>family-name: <textarea id="family-name"  placeholder="family-name"></textarea></label>
        <label>name: <textarea id="name"  placeholder="name"></textarea></label>
        <label>organization: <textarea id="organization"  placeholder="organization"></textarea></label>
        <label>street-address: <textarea id="street-address"  placeholder="street-address"></textarea></label>
        <label>address-line1: <textarea id="address-line1"  placeholder="address-line1"></textarea></label>
        <label>address-line2: <textarea id="address-line2"  placeholder="address-line2"></textarea></label>
        <label>address-line3: <textarea id="address-line3"  placeholder="address-line3"></textarea></label>
        <label>address-level3: <textarea id="address-level3"  placeholder="address-level3"></textarea></label>
        <label>address-level2: <textarea id="address-level2" placeholder="address-level2"></textarea></label>
        <label>address-level1: <textarea id="address-level1" placeholder="address-level1"></textarea></label>
        <label>postal-code: <textarea id="postal-code" placeholder="postal-code"></textarea></label>
        <label>country (abbr.): <textarea id="country" placeholder="country"></textarea></label>
        <label>country-name: <textarea id="country-name" placeholder="country-name"></textarea></label>
        <label>email: <textarea id="email" placeholder="email"></textarea></label>
        <label>tel: <textarea id="tel" placeholder="tel"></textarea></label>

        <label>email: <textarea id="email2" autocomplete="email"></textarea></label>
        <label>tel: <textarea id="tel2" autocomplete="tel"></textarea></label>
    </form>`,
    profile: TEST_PROFILE_ADDRESS,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          {
            fieldName: "street-address",
            autofill: TEST_PROFILE_ADDRESS["street-address"].replace("\n", " "),
          },
          // email and tel field with autocomplete attribute are only added to make this a valid section
          {
            fieldName: "email",
            reason: "autocomplete",
            autofill: TEST_PROFILE_ADDRESS.email,
          },
          {
            fieldName: "tel",
            reason: "autocomplete",
            autofill: TEST_PROFILE_ADDRESS.tel,
          },
        ],
      },
    ],
  },
  {
    description:
      "(credit card)textarea should only be restricted to specific field types when the field type is determined by heuristics",
    fixtureData: `<form>
        <label>cc-name: <textarea id="cc-name" placeholder="cc-name" autofocus></textarea></label>
        <label>cc-given-name: <textarea id="cc-given-name" placeholder="cc-given-name"></textarea></label>
        <label>cc-additional-name: <textarea id="cc-additional-name" placeholder="cc-additional-name"></textarea></label>
        <label>cc-family-name: <textarea id="cc-family-name" placeholder="cc-family-name"></textarea></label>
        <label>cc-number: <textarea id="cc-number" placeholder="cc-number"></textarea></label>
        <label>cc-exp-month: <textarea id="cc-exp-month" placeholder="cc-exp-month"></textarea></label>
        <label>cc-exp-year: <textarea id="cc-exp-year" placeholder="cc-exp-year"></textarea></label>
        <label>cc-exp: <textarea id="cc-exp" placeholder="cc-exp"></textarea></label>
        <label>cc-number: <textarea id="cc-number2" autocomplete="cc-number"></textarea></label>
    </form>`,
    profile: TEST_PROFILE_CREDIT_CARD,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          // cc-number field with autocomplete attribute is only added to make this a valid section
          {
            fieldName: "cc-number",
            autofill: TEST_PROFILE_CREDIT_CARD["cc-number"],
          },
        ],
      },
    ],
  },
]);

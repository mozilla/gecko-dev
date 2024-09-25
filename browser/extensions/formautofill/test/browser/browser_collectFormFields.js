/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test is ported from test/unit/test_collectFormFields.js

add_heuristic_tests([
  {
    description: "Form without autocomplete property",
    fixtureData: `<form>
               <input id="given-name">
               <input id="family-name">
               <input id="street-addr">
               <input id="city">
               <select id="country"></select>
               <input id='email'>
               <input id="phone">
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
          { fieldName: "address-level2" },
          { fieldName: "country" },
          { fieldName: "email" },
          { fieldName: "tel" },
        ],
      },
    ],
  },
  {
    description:
      "An address and credit card form with autocomplete properties and 1 token",
    fixtureData: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-address" autocomplete="street-address">
               <input id="address-level2" autocomplete="address-level2">
               <select id="country" autocomplete="country"></select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel">
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
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "address-level2" },
          { fieldName: "country" },
          { fieldName: "email" },
          { fieldName: "tel" },
        ],
      },
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
  {
    description: "An address form with autocomplete properties and 2 tokens",
    fixtureData: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-address" autocomplete="shipping street-address">
               <input id="address-level2" autocomplete="shipping address-level2">
               <input id="country" autocomplete="shipping country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { addressType: "shipping", fieldName: "given-name" },
          { addressType: "shipping", fieldName: "family-name" },
          { addressType: "shipping", fieldName: "street-address" },
          { addressType: "shipping", fieldName: "address-level2" },
          { addressType: "shipping", fieldName: "country" },
          { addressType: "shipping", fieldName: "email" },
          { addressType: "shipping", fieldName: "tel" },
        ],
      },
    ],
  },
  {
    description:
      "Form with autocomplete properties and profile is partly matched",
    fixtureData: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-address" autocomplete="shipping street-address">
               <input id="address-level2" autocomplete="shipping address-level2">
               <select id="country" autocomplete="shipping country"></select>
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { addressType: "shipping", fieldName: "given-name" },
          { addressType: "shipping", fieldName: "family-name" },
          { addressType: "shipping", fieldName: "street-address" },
          { addressType: "shipping", fieldName: "address-level2" },
          { addressType: "shipping", fieldName: "country" },
          { addressType: "shipping", fieldName: "email" },
          { addressType: "shipping", fieldName: "tel" },
        ],
      },
    ],
  },
  {
    description: "It's a valid address and credit card form.",
    fixtureData: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-address" autocomplete="shipping street-address">
               <input id="cc-number" autocomplete="shipping cc-number">
               </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { addressType: "shipping", fieldName: "given-name" },
          { addressType: "shipping", fieldName: "family-name" },
          { addressType: "shipping", fieldName: "street-address" },
        ],
      },
      {
        fields: [{ addressType: "shipping", fieldName: "cc-number" }],
      },
    ],
  },
  {
    description: "An invalid address form due to less than 3 fields.",
    fixtureData: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input autocomplete="shipping address-level2">
               </form>`,
    expectedResult: [
      {
        invalid: true,
        default: {
          reason: "autocomplete",
        },
        fields: [
          { addressType: "shipping", fieldName: "given-name" },
          { addressType: "shipping", fieldName: "address-level2" },
        ],
      },
    ],
  },
  /*
   * Valid Credit Card Form with autocomplete attribute
   */
  {
    description: "@autocomplete - A valid credit card form",
    fixtureData: `<form>
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-name" autocomplete="cc-name">
                 <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: "@autocomplete - A valid credit card form without cc-number",
    fixtureData: `<form>
                 <input id="cc-name" autocomplete="cc-name">
                 <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [{ fieldName: "cc-name" }, { fieldName: "cc-exp" }],
      },
    ],
  },
  {
    description: "@autocomplete - A valid cc-number only form",
    fixtureData: `<form><input id="cc-number" autocomplete="cc-number"></form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-number", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: "@autocomplete - A valid cc-name only form",
    fixtureData: `<form><input id="cc-name" autocomplete="cc-name"></form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-name", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: "@autocomplete - A valid cc-exp only form",
    fixtureData: `<form><input id="cc-exp" autocomplete="cc-exp"></form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-exp", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: "@autocomplete - A valid cc-exp-month + cc-exp-year form",
    fixtureData: `<form>
                 <input id="cc-exp-month" autocomplete="cc-exp-month">
                 <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-exp-month", reason: "autocomplete" },
          { fieldName: "cc-exp-year", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description: "@autocomplete - A valid cc-exp-month only form",
    fixtureData: `<form><input id="cc-exp-month" autocomplete="cc-exp-month"></form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-exp-month", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: "@autocomplete - A valid cc-exp-year only form",
    fixtureData: `<form><input id="cc-exp-year" autocomplete="cc-exp-year"></form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-exp-year", reason: "autocomplete" }],
      },
    ],
  },
  {
    description:
      "A valid credit card form without autocomplete attribute (cc-number is detected by fathom)",
    fixtureData: `<form>
                 <input id="cc-number" name="cc-number">
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-name", reason: "fathom" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
    ],
  },
  {
    description:
      "A valid credit card form without autocomplete attribute (only cc-number is detected by fathom)",
    fixtureData: `<form>
                 <input id="cc-number" name="cc-number">
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "fathom" },
          { fieldName: "cc-name", reason: "regex-heuristic" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.types",
        "cc-number",
      ],
    ],
  },
  {
    description:
      "A valid credit card form without autocomplete attribute (only cc-name is detected by fathom)",
    fixtureData: `<form>
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-name", reason: "fathom" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.types",
        "cc-name",
      ],
    ],
  },
  {
    description:
      "A credit card form is invalid when a fathom detected cc-number field is the only field in the form",
    fixtureData: `<form><input id="cc-number" name="cc-number"></form>`,
    expectedResult: [
      {
        invalid: true,
        fields: [{ fieldName: "cc-number", reason: "fathom" }],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.9",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
    ],
  },
  {
    description:
      "A credit card form is invalid when a fathom detected cc-name field is the only field in the form",
    fixtureData: `<form><input id="cc-name" name="cc-name"></form>`,
    expectedResult: [
      {
        invalid: true,
        fields: [{ fieldName: "cc-name", reason: "fathom" }],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.9",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
    ],
  },
  {
    description:
      "A cc-number only form is considered a valid credit card form when fathom is confident and there is no other <input> in the form",
    fixtureData: `<form><input id="cc-number" name="cc-number"></form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-number", reason: "fathom" }],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.95",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.99",
      ],
    ],
  },
  {
    description:
      "A cc-name only form is considered a valid credit card form when fathom is confident and there is no other <input> in the form",
    fixtureData: `<form><input id="cc-name" name="cc-name"></form>`,
    expectedResult: [
      {
        fields: [{ fieldName: "cc-name", reason: "fathom" }],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.95",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.99",
      ],
    ],
  },
  {
    description:
      "A credit card form is invalid when none of the fields are identified by fathom or autocomplete",
    fixtureData: `<form>
                 <input id="cc-number" name="cc-number">
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    expectedResult: [
      {
        invalid: true,
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
    ],
    prefs: [
      ["extensions.formautofill.creditCards.heuristics.fathom.types", ""],
    ],
  },
  //// Special Cases
  {
    description:
      "A credit card form with a high-confidence cc-name field is still considered invalid when there is another <input> field",
    fixtureData: `<form>
               <input id="cc-name" name="cc-name">
               <input id="password" type="password">
               </form>`,
    expectedResult: [
      {
        invalid: true,
        fields: [{ fieldName: "cc-name", reason: "fathom" }],
      },
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.95",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.96",
      ],
    ],
  },
  {
    description: "A valid credit card form with multiple cc-number fields",
    fixtureData: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
                </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", part: 1, reason: "fathom" },
          { fieldName: "cc-number", part: 2, reason: "fathom" },
          { fieldName: "cc-number", part: 3, reason: "fathom" },
          { fieldName: "cc-number", part: 4, reason: "fathom" },
          { fieldName: "cc-exp-month", reason: "autocomplete" },
          { fieldName: "cc-exp-year", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description: "Three sets of adjacent phone number fields",
    fixtureData: `<form>
                 <input id="shippingAC" name="phone" maxlength="3">
                 <input id="shippingPrefix" name="phone" maxlength="3">
                 <input id="shippingSuffix" name="phone" maxlength="4">
                 <input id="shippingTelExt" name="extension">

                 <input id="billingAC" name="phone" maxlength="3">
                 <input id="billingPrefix" name="phone" maxlength="3">
                 <input id="billingSuffix" name="phone" maxlength="4">

                 <input id="otherCC" name="phone" maxlength="3">
                 <input id="otherAC" name="phone" maxlength="3">
                 <input id="otherPrefix" name="phone" maxlength="3">
                 <input id="otherSuffix" name="phone" maxlength="4">
               </form>`,
    expectedResult: [
      {
        default: {
          reason: "update-heuristic",
        },
        fields: [
          { fieldName: "tel-area-code" },
          { fieldName: "tel-local-prefix" },
          { fieldName: "tel-local-suffix" },
          { fieldName: "tel-extension" },
        ],
      },
      {
        default: {
          reason: "update-heuristic",
        },
        fields: [
          { fieldName: "tel-area-code" },
          { fieldName: "tel-local-prefix" },
          { fieldName: "tel-local-suffix" },

          // TODO Bug 1421181 - "tel-country-code" field should belong to the next
          // section. There should be a way to group the related fields during the
          // parsing stage.
          { fieldName: "tel-country-code" },
        ],
      },
      {
        default: {
          reason: "update-heuristic",
        },
        fields: [
          { fieldName: "tel-area-code" },
          { fieldName: "tel-local-prefix" },
          { fieldName: "tel-local-suffix" },
        ],
      },
    ],
  },
  {
    description:
      "Do not dedup the same field names of the different telephone fields.",
    fixtureData: `<form>
                 <input id="i1" autocomplete="given-name">
                 <input id="i2" autocomplete="family-name">
                 <input id="i3" autocomplete="street-address">
                 <input id="i4" autocomplete="email">

                 <input id="homePhone" maxlength="10">
                 <input id="mobilePhone" maxlength="10">
                 <input id="officePhone" maxlength="10">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "email" },
          { fieldName: "tel", reason: "regex-heuristic" },
          { fieldName: "tel", reason: "regex-heuristic" },
          { fieldName: "tel", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description:
      "The duplicated phones of a single one and a set with ac, prefix, suffix.",
    fixtureData: `<form>
                 <input id="i1" autocomplete="shipping given-name">
                 <input id="i2" autocomplete="shipping family-name">
                 <input id="i3" autocomplete="shipping street-address">
                 <input id="i4" autocomplete="shipping email">
                 <input id="singlePhone" autocomplete="shipping tel">
                 <input id="shippingAreaCode" autocomplete="shipping tel-area-code">
                 <input id="shippingPrefix" autocomplete="shipping tel-local-prefix">
                 <input id="shippingSuffix" autocomplete="shipping tel-local-suffix">
               </form>`,
    expectedResult: [
      {
        fields: [
          { addressType: "shipping", fieldName: "given-name" },
          { addressType: "shipping", fieldName: "family-name" },
          { addressType: "shipping", fieldName: "street-address" },
          { addressType: "shipping", fieldName: "email" },

          // NOTES: Ideally, there is only one full telephone field(s) in a form for
          // this case. We can see if there is any better solution later.
          { addressType: "shipping", fieldName: "tel" },
          { addressType: "shipping", fieldName: "tel-area-code" },
          { addressType: "shipping", fieldName: "tel-local-prefix" },
          { addressType: "shipping", fieldName: "tel-local-suffix" },
        ],
      },
    ],
  },
  {
    description: "Always adopt the info from autocomplete attribute.",
    fixtureData: `<form>
                 <input id="given-name" autocomplete="shipping given-name">
                 <input id="family-name" autocomplete="shipping family-name">
                 <input id="dummyAreaCode" autocomplete="shipping tel" maxlength="3">
                 <input id="dummyPrefix" autocomplete="shipping tel" maxlength="3">
                 <input id="dummySuffix" autocomplete="shipping tel" maxlength="4">
               </form>`,
    expectedResult: [
      {
        fields: [
          { addressType: "shipping", fieldName: "given-name" },
          { addressType: "shipping", fieldName: "family-name" },
          { addressType: "shipping", fieldName: "tel" },
          { addressType: "shipping", fieldName: "tel" },
          { addressType: "shipping", fieldName: "tel" },
        ],
      },
    ],
  },
  /**
   * The following are ported from test_getFormInputDetails.js
   */
  {
    description: "Form containing 5 fields with autocomplete attribute.",
    fixtureData: `<form id="form1">
                 <input id="street-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <select id="country" autocomplete="country"></select>
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    expectedResult: [
      {
        fields: [
          {
            section: "",
            addressType: "",
            contactType: "",
            fieldName: "street-address",
          },
          {
            section: "",
            addressType: "",
            contactType: "",
            fieldName: "address-level2",
          },
          {
            section: "",
            addressType: "",
            contactType: "",
            fieldName: "country",
          },
          { section: "", addressType: "", contactType: "", fieldName: "email" },
          { section: "", addressType: "", contactType: "", fieldName: "tel" },
        ],
      },
    ],
  },
  {
    description: "2 forms that are able to be auto filled",
    fixtureData: `<form id="form2">
                 <input id="home-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <select id="country" autocomplete="country"></select>
               </form>
               <form id="form3">
                 <input id="office-addr" autocomplete="street-address">
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "street-address" },
          { fieldName: "address-level2" },
          { fieldName: "country" },
        ],
      },
      {
        fields: [
          { fieldName: "street-address" },
          { fieldName: "email" },
          { fieldName: "tel" },
        ],
      },
    ],
  },
]);

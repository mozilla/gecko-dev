/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
requestLongerTimeout(2);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
});

add_heuristic_tests([
  {
    description: `(FR) name should be updated to family-name in context of given-name`,
    fixtureData: `
          <form>
               <input id="lastName">
               <label for="lastName" >Nom</label>
               <input id="firstName">
               <label for="firstName" >Prénom</label>
               <label for="middleName" >"Middle"</label>
               <input id=middleName>
               <label for="phoneNumber" >"N° de téléphone"</label>
               <input id=phoneNumber>
           </form>`,

    expectedResult: [
      {
        fields: [
          {
            fieldName: "family-name",
            reason: "update-heuristic",
          },
          {
            fieldName: "given-name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "additional-name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "tel",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `(FR) name should be updated to family-name in context of given-name`,
    fixtureData: `
          <form>
               <input id="lastName">
               <label for="lastName" >Nom</label>
               <input id="firstName">
               <label for="firstName" >Prénom</label>
               <input id=middleName>
               <label for="middleName" >"Middle"</label>
           </form>`,

    expectedResult: [
      {
        fields: [
          {
            fieldName: "family-name",
            reason: "update-heuristic",
          },
          {
            fieldName: "given-name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "additional-name",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `(DE) name should be updated to family-name in context of given-name`,
    fixtureData: `
           <form>
               <input placeholder="Name">
               <input placeholder="Vorname">
               <input placeholder="Address">
           </form>`,

    expectedResult: [
      {
        fields: [
          {
            fieldName: "family-name",
            reason: "update-heuristic",
          },
          {
            fieldName: "given-name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "street-address",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `(DE) name should not be updated to family-name if given-name not present`,
    fixtureData: `
           <form>
               <input placeholder="Name">
               <input placeholder="Address">
               <input placeholder="City">
           </form>`,

    expectedResult: [
      {
        fields: [
          {
            fieldName: "name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "street-address",
            reason: "regex-heuristic",
          },
          {
            fieldName: "address-level2",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `(DE) name should be corrected to family-name in context of given-name`,
    fixtureData: `
               <form>
                   <input placeholder="Vorname">
                   <input placeholder="Name">
                   <input placeholder="email">
               </form>`,

    expectedResult: [
      {
        fields: [
          {
            fieldName: "given-name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "family-name",
            reason: "update-heuristic",
          },
          {
            fieldName: "email",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `name should be corrected to family-name in context of given-name (1)`,
    fixtureData: `
               <form>
                   <input placeholder="First Name">
                   <input placeholder="Name">
                   <input placeholder="email">
               </form>`,

    expectedResult: [
      {
        fields: [
          {
            fieldName: "given-name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "family-name",
            reason: "update-heuristic",
          },
          {
            fieldName: "email",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `name should be corrected to family-name in context of given-name (2)`,
    fixtureData: `
               <form>
                   <input placeholder="Name">
                   <input placeholder="First Name">
                   <input placeholder="email">
               </form>`,

    expectedResult: [
      {
        fields: [
          {
            fieldName: "family-name",
            reason: "update-heuristic",
          },
          {
            fieldName: "given-name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "email",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `two adjacent name sections: name(0) is invalid and not filled, name(1) is a new section and detected as name`,
    fixtureData: `
               <form>
                   <input placeholder="Name">
                   <input placeholder="Name">
                   <input placeholder="Email">
                   <input placeholder="Phone">
               </form>`,

    expectedResult: [
      {
        invalid: true,
        fields: [
          {
            fieldName: "name",
            reason: "regex-heuristic",
          },
        ],
      },
      {
        fields: [
          {
            fieldName: "name",
            reason: "regex-heuristic",
          },
          {
            fieldName: "email",
            reason: "regex-heuristic",
          },
          {
            fieldName: "tel",
            reason: "regex-heuristic",
          },
        ],
      },
    ],
  },
  {
    description: `two names with identical firstname ids, with name=lastname second`,
    fixtureData: `
               <form>
                   <input id="firstname" name="firstname">
                   <input id="firstname" name="lastname">
                   <input id="email">
                   <input id="tel">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "family-name", reason: "update-heuristic" },
          { fieldName: "email", reason: "regex-heuristic" },
          { fieldName: "tel", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description: `two names with identical firstname ids, with name=lastname first`,
    fixtureData: `
               <form>
                   <input id="firstname" name="lastname">
                   <input id="firstname" name="firstname">
                   <input id="email">
                   <input id="tel">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "family-name", reason: "update-heuristic" },
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "email", reason: "regex-heuristic" },
          { fieldName: "tel", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description: `two names with identical firstname ids, with labels`,
    fixtureData: `
               <form>
                   <label>Last Name: <input name="firstname"></label>
                   <label>First Name: <input name="firstname"></label>
                   <input id="email">
                   <input id="tel">
               </form>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "family-name", reason: "update-heuristic" },
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "email", reason: "regex-heuristic" },
          { fieldName: "tel", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description: `two names with identical firstname ids, no name`,
    fixtureData: `
               <form>
                   <input id="firstname">
                   <input id="firstname">
                   <input id="email">
                   <input id="tel">
               </form>`,
    expectedResult: [
      {
        invalid: true,
        fields: [{ fieldName: "given-name", reason: "regex-heuristic" }],
      },
      {
        fields: [
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "email", reason: "regex-heuristic" },
          { fieldName: "tel", reason: "regex-heuristic" },
        ],
      },
    ],
  },
]);

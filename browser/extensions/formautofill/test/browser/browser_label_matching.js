/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_heuristic_tests([
  {
    description:
      "Multiple inputs with the same id — <label> should only match the first occurrence",
    fixtureData: `<form>
               <input id='email' autocomplete="email">
               <input id='name' autocomplete="name">
               <label for="inputA">Street</label>
               <input id="inputA">
               <label for="inputA">Organization</label>
               <input id="inputA">
               <label for="inputA">Country</label>
               <input id="inputA">
               </form>`,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email" },
          { fieldName: "name" },
          { fieldName: "street-address", reason: "regex-heuristic" },
        ],
      },
    ],
  },
]);

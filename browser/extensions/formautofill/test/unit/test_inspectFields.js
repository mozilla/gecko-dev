/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormAutofillChild } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofillChild.sys.mjs"
);

add_task(async function test_inspectFields() {
  const doc = MockDocument.createTestDocument(
    "http://localhost:8080/test/",
    `<form>
       <input id="cc-number" autocomplete="cc-number">
       <input id="cc-name" autocomplete="cc-name">
       <input id="cc-exp" autocomplete="cc-exp">
     </form>
     <select id="cc-type" autocomplete="cc-type">
       <option/>
       <option value="visa">VISA</option>
     </select>
     <form>
       <input id="name" autocomplete="name">
       <select id="country" autocomplete="country">
         <option/>
         <option value="US">United States</option>
       </select>
     </form>
     <input id="email" autocomplete="email">
     <input id="unknown" autocomplete="unknown">
    `
  );
  const fac = new FormAutofillChild();
  sinon.stub(fac, "document").get(() => {
    return doc;
  });
  sinon.stub(fac, "browsingContext").get(() => {
    return {};
  });

  const fields = fac.inspectFields();

  const expectedElements = Array.from(doc.querySelectorAll("input, select"));
  const inspectedElements = fields.map(field => field.element);
  Assert.deepEqual(
    expectedElements,
    inspectedElements,
    "inspectedElements should return all the eligible fields"
  );

  sinon.restore();
});

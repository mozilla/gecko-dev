"use strict";

const { FormStateManager } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormStateManager.sys.mjs"
);

const TESTCASES = [
  {
    description: "Form containing 5 fields with autocomplete attribute.",
    document: `<form id="form1">
                 <input id="street-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <select id="country" autocomplete="country"></select>
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    targetInput: ["street-addr", "email"],
    expectedResult: [
      {
        input: {
          section: "",
          addressType: "",
          contactType: "",
          fieldName: "street-address",
        },
        formId: "form1",
        form: [
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
      {
        input: {
          section: "",
          addressType: "",
          contactType: "",
          fieldName: "email",
        },
        formId: "form1",
        form: [
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
    document: `<form id="form2">
                 <input id="home-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <select id="country" autocomplete="country"></select>
               </form>
               <form id="form3">
                 <input id="office-addr" autocomplete="street-address">
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    targetInput: ["home-addr", "office-addr"],
    expectedResult: [
      {
        input: {
          section: "",
          addressType: "",
          contactType: "",
          fieldName: "street-address",
        },
        formId: "form2",
        form: [
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
        ],
      },
      {
        input: {
          section: "",
          addressType: "",
          contactType: "",
          fieldName: "street-address",
        },
        formId: "form3",
        form: [
          {
            section: "",
            addressType: "",
            contactType: "",
            fieldName: "street-address",
          },
          { section: "", addressType: "", contactType: "", fieldName: "email" },
          { section: "", addressType: "", contactType: "", fieldName: "tel" },
        ],
      },
    ],
  },
];

function inputDetailAssertion(detail, expected) {
  Assert.equal(detail.section, expected.section);
  Assert.equal(detail.addressType, expected.addressType);
  Assert.equal(detail.contactType, expected.contactType);
  Assert.equal(detail.fieldName, expected.fieldName);
  Assert.equal(detail.element, expected.elementWeakRef.deref());
}

TESTCASES.forEach(testcase => {
  add_task(async function () {
    info("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );

    for (const i in testcase.targetInput) {
      const input = doc.getElementById(testcase.targetInput[i]);
      const fsm = new FormStateManager();
      fsm.identifyAutofillFields(input);

      const formDetails = testcase.expectedResult[i].form;
      for (const formDetail of formDetails) {
        // Compose a query string to get the exact reference of <input>/<select>
        // element, e.g. #form1 > *[autocomplete="street-address"]
        let queryString =
          "#" +
          testcase.expectedResult[i].formId +
          " > *[autocomplete=" +
          formDetail.fieldName +
          "]";
        formDetail.elementWeakRef = new WeakRef(doc.querySelector(queryString));
      }

      const handler = fsm.getFormHandler(input);
      handler.fieldDetails.forEach((detail, index) => {
        inputDetailAssertion(detail, formDetails[index]);
      });
    }
  });
});

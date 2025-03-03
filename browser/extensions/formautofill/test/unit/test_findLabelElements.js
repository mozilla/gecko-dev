"use strict";

var { LabelUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/LabelUtils.sys.mjs"
);

const TESTCASES = [
  {
    description: "Input contains in a label element.",
    document: `<form>
                 <label id="labelA"> label type A
                   <input id="typeA" type="text">
                 </label>
               </form>`,
    expectedLabelIds: [["labelA"]],
  },
  {
    description: "Input contains in a label element.",
    document: `<label id="labelB"> label type B
                 <div> inner div
                   <input id="typeB" type="text">
                 </div>
               </label>`,
    inputId: "typeB",
    expectedLabelIds: [["labelB"]],
  },
  {
    description: '"for" attribute used to indicate input by one label.',
    document: `<label id="labelC" for="typeC">label type C</label>
               <input id="typeC" type="text">`,
    inputId: "typeC",
    expectedLabelIds: [["labelC"]],
  },
  {
    description: '"for" attribute used to indicate input by multiple labels.',
    document: `<form>
                 <label id="labelD1" for="typeD">label type D1</label>
                 <label id="labelD2" for="typeD">label type D2</label>
                 <label id="labelD3" for="typeD">label type D3</label>
                 <input id="typeD" type="text">
               </form>`,
    inputId: "typeD",
    expectedLabelIds: [["labelD1", "labelD2", "labelD3"]],
  },
  {
    description:
      '"for" attribute used to indicate input by multiple labels with space prefix/postfix.',
    document: `<label id="labelE1" for="typeE">label type E1</label>
               <label id="labelE2" for="typeE  ">label type E2</label>
               <label id="labelE3" for="  TYPEe">label type E3</label>
               <label id="labelE4" for="  typeE  ">label type E4</label>
               <input id="   typeE  " type="text">`,
    inputId: "   typeE  ",
    expectedLabelIds: [[]],
  },
  {
    description: "Input contains in a label element.",
    document: `<label id="labelF"> label type F
                 <label for="dummy"> inner label
                   <input id="typeF" type="text">
                   <input id="dummy" type="text">
                 </div>
               </label>`,
    inputId: "typeF",
    expectedLabelIds: [["labelF"], [""]],
  },
  {
    description:
      '"for" attribute used to indicate input by labels out of the form.',
    document: `<label id="labelG1" for="typeG">label type G1</label>
               <form>
                 <label id="labelG2" for="typeG">label type G2</label>
                 <input id="typeG" type="text">
               </form>
               <label id="labelG3" for="typeG">label type G3</label>`,
    inputId: "typeG",
    expectedLabelIds: [["labelG1", "labelG2", "labelG3"]],
  },
  {
    description:
      "labels with no for attribute or child with one input at a different level",
    document: `<form>
                 <label id="labelH1">label H1</label>
                 <input>
                 <label id="labelH2">label H2</label>
                 <div><span><input></span></div>
               </form>`,
    inputId: "labelH1",
    expectedLabelIds: [["labelH1"], ["labelH2"]],
  },
  {
    description:
      "labels with no for attribute or child with an input and button",
    document: `<form>
                 <label id="labelI1">label I1</label>
                 <input>
                 <label id="labelI2">label I2</label>
                 <button>
                 <input>
               </form>`,
    inputId: "labelI1",
    expectedLabelIds: [["labelI1"], []],
  },
  {
    description: "three labels with no for attribute or child.",
    document: `<form>
                 <button>
                 <label id="labelJ1">label J1</label>
                 <label id="labelJ2">label J2</label>
                 <input>
                 <label id="labelJ3">label J3</label>
                 <meter>
                 <input>
               </form>`,
    inputId: "labelJ1",
    expectedLabelIds: [["labelJ2"], []],
  },
  {
    description: "four labels with no for attribute or child.",
    document: `<form>
                 <input>
                 <fieldset>
                   <label id="labelK1">label K1</label>
                   <label id="labelK2">label K2</label>
                   <input>
                   <label id="labelK3">label K3</label>
                   <div><b><input></b></div>
                   <label id="labelK4">label K4</label>
                 </fieldset>
                 <input>
               </form>`,
    inputId: "labelK1",
    expectedLabelIds: [[], ["labelK2"], ["labelK3"], []],
  },
  {
    description:
      "labels with no for attribute or child and inputs at different level.",
    document: `<form>
                 <input>
                 <div><span><input></span></div>
                 <label id="labelL1">label L1</label>
                 <label id="labelL2">label L2</label>
                 <div><span><input></span></div>
                 </input>
               </form>`,
    inputId: "labelK1",
    expectedLabelIds: [[], [], ["labelL2"], []],
  },
];

TESTCASES.forEach(testcase => {
  add_task(async function () {
    info("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );

    let formElements = doc.querySelectorAll("input", "select");
    let labelsIndex = 0;
    for (let formElement of formElements) {
      let labels = LabelUtils.findLabelElements(formElement);
      Assert.deepEqual(
        labels.map(l => l.id),
        testcase.expectedLabelIds[labelsIndex++]
      );
    }

    LabelUtils.clearLabelMap();
  });
});

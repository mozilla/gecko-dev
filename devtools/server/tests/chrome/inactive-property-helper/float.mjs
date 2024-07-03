/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `float` test cases.
export default [
  {
    info: "display: inline is inactive on a floated element",
    property: "display",
    tagName: "div",
    rules: ["div { display: inline; float: right; }"],
    isActive: false,
    expectedMsgId: "inactive-css-not-display-block-on-floated-2",
  },
  {
    info: "display: block is active on a floated element",
    property: "display",
    tagName: "div",
    rules: ["div { display: block; float: right;}"],
    isActive: true,
  },
  {
    info: "display: inline-grid is inactive on a floated element",
    property: "display",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      container.classList.add("test");
      rootNode.append(container);
      return container;
    },
    rules: [
      "div { float: left; display:block; }",
      ".test { display: inline-grid ;}",
    ],
    isActive: false,
    expectedMsgId: "inactive-css-not-display-block-on-floated-2",
  },
  {
    info: "display: table-footer-group is inactive on a floated element",
    property: "display",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      container.style.display = "table";
      const footer = document.createElement("div");
      footer.classList.add("table-footer");
      container.append(footer);
      rootNode.append(container);
      return footer;
    },
    rules: [".table-footer { display: table-footer-group; float: left;}"],
    isActive: false,
    expectedMsgId: "inactive-css-not-display-block-on-floated-2",
  },
  createGridPlacementOnFloatedItemTest("grid-row"),
  createGridPlacementOnFloatedItemTest("grid-column"),
  createGridPlacementOnFloatedItemTest("grid-area", "foo"),
  {
    info: "float is valid on block-level elements",
    property: "float",
    tagName: "div",
    rules: ["div { float: right; }"],
    isActive: true,
  },
  {
    info: "float is invalid on flex items",
    property: "float",
    createTestElement: createContainerWithItem("flex"),
    rules: [".item { float: right; }"],
    isActive: false,
    expectedMsgId: "inactive-css-only-non-grid-or-flex-item",
  },
  {
    info: "float is invalid on grid items",
    property: "float",
    createTestElement: createContainerWithItem("grid"),
    rules: [".item { float: right; }"],
    isActive: false,
    expectedMsgId: "inactive-css-only-non-grid-or-flex-item",
  },
  {
    info: "clear is valid on block-level elements",
    property: "clear",
    tagName: "div",
    rules: ["div { clear: right; }"],
    isActive: true,
  },
  {
    info: "clear is valid on block-level grid containers",
    property: "clear",
    tagName: "div",
    rules: ["div { display: grid; clear: left; }"],
    isActive: true,
  },
  {
    info: "clear is invalid on non-block-level elements",
    property: "clear",
    tagName: "span",
    rules: ["span { clear: right; }"],
    isActive: false,
    expectedMsgId: "inactive-css-not-block",
  },
  {
    info: "shape-image-threshold is valid on floating elements",
    property: "shape-image-threshold",
    tagName: "div",
    rules: ["div { shape-image-threshold: 0.5; float: right; }"],
    isActive: true,
  },
  {
    info: "shape-image-threshold is invalid on non-floating elements",
    property: "shape-image-threshold",
    tagName: "div",
    rules: ["div { shape-image-threshold: 0.5; }"],
    isActive: false,
    expectedMsgId: "inactive-css-not-floated",
  },
  {
    info: "shape-outside is valid on floating elements",
    property: "shape-outside",
    tagName: "div",
    rules: ["div { shape-outside: circle(); float: right; }"],
    isActive: true,
  },
  {
    info: "shape-outside is invalid on non-floating elements",
    property: "shape-outside",
    tagName: "div",
    rules: ["div { shape-outside: circle(); }"],
    isActive: false,
    expectedMsgId: "inactive-css-not-floated",
  },
  {
    info: "shape-margin is valid on floating elements",
    property: "shape-margin",
    tagName: "div",
    rules: ["div { shape-margin: 10px; float: right; }"],
    isActive: true,
  },
  {
    info: "shape-margin is invalid on non-floating elements",
    property: "shape-margin",
    tagName: "div",
    rules: ["div { shape-margin: 10px; }"],
    isActive: false,
    expectedMsgId: "inactive-css-not-floated",
  },
];

function createGridPlacementOnFloatedItemTest(property, value = "2") {
  return {
    info: `grid placement property ${property} is active on a floated grid item`,
    property,
    createTestElement: rootNode => {
      const grid = document.createElement("div");
      grid.style.display = "grid";
      grid.style.gridTemplateRows = "repeat(5, 1fr)";
      grid.style.gridTemplateColumns = "repeat(5, 1fr)";
      grid.style.gridTemplateAreas = "'foo foo foo'";
      rootNode.appendChild(grid);

      const item = document.createElement("span");
      grid.appendChild(item);

      return item;
    },
    rules: [`span { ${property}: ${value}; float: left; }`],
    isActive: true,
  };
}

function createContainerWithItem(display) {
  return rootNode => {
    const container = document.createElement("div");
    container.style.display = display;
    const item = document.createElement("div");
    item.classList.add("item");
    container.append(item);
    rootNode.append(container);
    return item;
  };
}

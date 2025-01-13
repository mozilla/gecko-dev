/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that searching for elements using the inspector search field
// produces correct suggestions.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

// See head.js `checkMarkupSearchSuggestions` function
const TEST_DATA = [
  {
    key: "d",
    value: "d",
    suggestions: ["div", "#d1", "#d2"],
  },
  {
    key: "i",
    value: "di",
    suggestions: ["div"],
  },
  {
    key: "v",
    value: "div",
    suggestions: [],
  },
  {
    key: ".",
    value: "div.",
    suggestions: ["div.c1"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "div",
    suggestions: [],
  },
  {
    key: "#",
    value: "div#",
    suggestions: ["div#d1", "div#d2"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "div",
    suggestions: [],
  },
  {
    key: "VK_BACK_SPACE",
    value: "di",
    suggestions: ["div"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "d",
    suggestions: ["div", "#d1", "#d2"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "",
    suggestions: [],
  },
  {
    key: ".",
    value: ".",
    suggestions: [".c1", ".c2"],
  },
  {
    key: "c",
    value: ".c",
    suggestions: [".c1", ".c2"],
  },
  {
    key: "2",
    value: ".c2",
    suggestions: [],
  },
  {
    key: "VK_BACK_SPACE",
    value: ".c",
    suggestions: [".c1", ".c2"],
  },
  {
    key: "1",
    value: ".c1",
    suggestions: [],
  },
  {
    key: "#",
    value: ".c1#",
    suggestions: ["#d2", "#p1", "#s2"],
  },
  {
    key: "VK_BACK_SPACE",
    value: ".c1",
    suggestions: [],
  },
  {
    key: "VK_BACK_SPACE",
    value: ".c",
    suggestions: [".c1", ".c2"],
  },
  {
    key: "VK_BACK_SPACE",
    value: ".",
    suggestions: [".c1", ".c2"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "",
    suggestions: [],
  },
  {
    key: "#",
    value: "#",
    suggestions: [
      "#b1",
      "#d1",
      "#d2",
      "#p1",
      "#p2",
      "#p3",
      "#root",
      "#s1",
      "#s2",
    ],
  },
  {
    key: "p",
    value: "#p",
    suggestions: ["#p1", "#p2", "#p3"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "#",
    suggestions: [
      "#b1",
      "#d1",
      "#d2",
      "#p1",
      "#p2",
      "#p3",
      "#root",
      "#s1",
      "#s2",
    ],
  },
  {
    key: "VK_BACK_SPACE",
    value: "",
    suggestions: [],
  },
  {
    key: "p",
    value: "p",
    suggestions: ["p", "#p1", "#p2", "#p3"],
  },
  {
    key: "[",
    value: "p[",
    suggestions: [],
  },
  {
    key: "i",
    value: "p[i",
    suggestions: [],
  },
  {
    key: "d",
    value: "p[id",
    suggestions: [],
  },
  {
    key: "*",
    value: "p[id*",
    suggestions: [],
  },
  {
    key: "=",
    value: "p[id*=",
    suggestions: [],
  },
  {
    key: "p",
    value: "p[id*=p",
    suggestions: [],
  },
  {
    key: "]",
    value: "p[id*=p]",
    suggestions: [],
  },
  {
    key: ".",
    value: "p[id*=p].",
    suggestions: ["p[id*=p].c1", "p[id*=p].c2"],
  },
  {
    key: "VK_BACK_SPACE",
    value: "p[id*=p]",
    suggestions: [],
  },
  {
    key: "#",
    value: "p[id*=p]#",
    suggestions: ["p[id*=p]#p1", "p[id*=p]#p2", "p[id*=p]#p3"],
  },
];

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);
  await checkMarkupSearchSuggestions(inspector, TEST_DATA);
});

add_task(async function () {
  const { inspector } =
    await openInspectorForURL(`data:text/html,<meta charset=utf8>
    <main>
      <div class="testA_alpha"></div>
      <div class="test1_numeric"></div>
      <div class="test-_dash"></div>
      <div class="test__underscore"></div>
      <test-foo></test-element>
      <test-bar></test-element>
    </main>`);

  const testSuggestions = [
    "test-bar",
    "test-foo",
    ".test__underscore",
    ".test-_dash",
    ".test1_numeric",
    ".testA_alpha",
  ];

  await checkMarkupSearchSuggestions(inspector, [
    {
      key: "t",
      value: "t",
      suggestions: testSuggestions,
    },
    {
      key: "e",
      value: "te",
      suggestions: testSuggestions,
    },
    {
      key: "s",
      value: "tes",
      suggestions: testSuggestions,
    },
    {
      key: "t",
      value: "test",
      suggestions: testSuggestions,
    },
    {
      key: "-",
      value: "test-",
      suggestions: ["test-bar", "test-foo", ".test-_dash"],
    },
    {
      key: "f",
      value: "test-f",
      suggestions: ["test-foo"],
    },
    {
      key: "VK_BACK_SPACE",
      value: "test-",
      suggestions: ["test-bar", "test-foo", ".test-_dash"],
    },
    {
      key: "VK_BACK_SPACE",
      value: "test",
      suggestions: testSuggestions,
    },
    {
      key: "_",
      value: "test_",
      suggestions: [".test__underscore"],
    },
    {
      key: "VK_BACK_SPACE",
      value: "test",
      suggestions: testSuggestions,
    },
    {
      key: "1",
      value: "test1",
      suggestions: [".test1_numeric"],
    },
    {
      key: "VK_BACK_SPACE",
      value: "test",
      suggestions: testSuggestions,
    },
    {
      key: "A",
      value: "testA",
      suggestions: [".testA_alpha"],
    },
  ]);
});

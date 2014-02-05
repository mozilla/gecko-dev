/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test expected outputs of the output-parser's parseCssProperty function.

// This is more of a unit test than a mochitest-browser test, but can't be
// tested with an xpcshell test as the output-parser requires the DOM to work.

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let {OutputParser} = devtools.require("devtools/output-parser");

const COLOR_CLASS = "color-class";
const URL_CLASS = "url-class";

function test() {
  waitForExplicitFinish();

  let testData = [
    {
      name: "width",
      value: "100%",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 0);
        is(fragment.textContent, "100%");
      }
    },
    {
      name: "width",
      value: "blue",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 0);
      }
    },
    {
      name: "content",
      value: "'red url(test.png) repeat top left'",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 0);
      }
    },
    {
      name: "content",
      value: "\"blue\"",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 0);
      }
    },
    {
      name: "margin-left",
      value: "url(something.jpg)",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 0);
      }
    },
    {
      name: "background-color",
      value: "transparent",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 1);
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.textContent, "transparent");
      }
    },
    {
      name: "color",
      value: "red",
      test: fragment => {
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.textContent, "red");
      }
    },
    {
      name: "color",
      value: "#F06",
      test: fragment => {
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.textContent, "#F06");
      }
    },
    {
      name: "border-top-left-color",
      value: "rgba(14, 255, 20, .5)",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 1);
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.textContent, "rgba(14, 255, 20, .5)");
      }
    },
    {
      name: "border",
      value: "80em dotted pink",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 1);
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.querySelector("." + COLOR_CLASS).textContent, "pink");
      }
    },
    {
      name: "color",
      value: "red !important",
      test: fragment => {
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.textContent, "red !important");
      }
    },
    {
      name: "background",
      value: "red url(test.png) repeat top left",
      test: fragment => {
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.querySelectorAll("." + URL_CLASS).length, 1);
        is(fragment.querySelector("." + COLOR_CLASS).textContent, "red");
        is(fragment.querySelector("." + URL_CLASS).textContent, "test.png");
        is(fragment.querySelectorAll("*").length, 2);
      }
    },
    {
      name: "background",
      value: "blue url(test.png) repeat top left !important",
      test: fragment => {
        is(fragment.querySelectorAll("." + COLOR_CLASS).length, 1);
        is(fragment.querySelectorAll("." + URL_CLASS).length, 1);
        is(fragment.querySelector("." + COLOR_CLASS).textContent, "blue");
        is(fragment.querySelector("." + URL_CLASS).textContent, "test.png");
        is(fragment.querySelectorAll("*").length, 2);
      }
    },
    {
      name: "list-style-image",
      value: "url(\"images/arrow.gif\")",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 1);
        is(fragment.querySelector("." + URL_CLASS).textContent, "images/arrow.gif");
      }
    },
    {
      name: "list-style-image",
      value: "url(\"images/arrow.gif\")!important",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 1);
        is(fragment.querySelector("." + URL_CLASS).textContent, "images/arrow.gif");
        is(fragment.textContent, "url('images/arrow.gif')!important");
      }
    },
    {
      name: "-moz-binding",
      value: "url(http://somesite.com/path/to/binding.xml#someid)",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 1);
        is(fragment.querySelectorAll("." + URL_CLASS).length, 1);
        is(fragment.querySelector("." + URL_CLASS).textContent, "http://somesite.com/path/to/binding.xml#someid");
      }
    },
    {
      name: "background",
      value: "linear-gradient(to right, rgba(183,222,237,1) 0%, rgba(33,180,226,1) 30%, rgba(31,170,217,.5) 44%, #F06 75%, red 100%)",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 5);
        let allSwatches = fragment.querySelectorAll("." + COLOR_CLASS);
        is(allSwatches.length, 5);
        is(allSwatches[0].textContent, "rgba(183,222,237,1)");
        is(allSwatches[1].textContent, "rgba(33,180,226,1)");
        is(allSwatches[2].textContent, "rgba(31,170,217,.5)");
        is(allSwatches[3].textContent, "#F06");
        is(allSwatches[4].textContent, "red");
      }
    },
    {
      name: "background",
      value: "-moz-radial-gradient(center 45deg, circle closest-side, orange 0%, red 100%)",
      test: fragment => {
        is(fragment.querySelectorAll("*").length, 2);
        let allSwatches = fragment.querySelectorAll("." + COLOR_CLASS);
        is(allSwatches.length, 2);
        is(allSwatches[0].textContent, "orange");
        is(allSwatches[1].textContent, "red");
      }
    }
  ];

  let parser = new OutputParser();
  for (let i = 0; i < testData.length; i ++) {
    let data = testData[i];
    info("Output-parser test data " + i + ". {" + data.name + " : " + data.value + ";}");
    data.test(parser.parseCssProperty(data.name, data.value, {
      colorClass: COLOR_CLASS,
      urlClass: URL_CLASS,
      defaultColorType: false
    }));
  }

  finish();
}

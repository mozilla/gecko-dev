/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getLoadContext() {
  var Ci = SpecialPowers.Ci;
  return SpecialPowers.wrap(window).docShell.QueryInterface(Ci.nsILoadContext);
}

var clipboard = SpecialPowers.Services.clipboard;
var documentViewer = SpecialPowers.wrap(
  window
).docShell.docViewer.QueryInterface(SpecialPowers.Ci.nsIDocumentViewerEdit);

function getClipboardData(mime) {
  var transferable = SpecialPowers.Cc[
    "@mozilla.org/widget/transferable;1"
  ].createInstance(SpecialPowers.Ci.nsITransferable);
  transferable.init(getLoadContext());
  transferable.addDataFlavor(mime);
  clipboard.getData(
    transferable,
    1,
    SpecialPowers.wrap(window).browsingContext.currentWindowContext
  );
  var data = SpecialPowers.createBlankObject();
  transferable.getTransferData(mime, data);
  return data;
}

function testClipboardValue(suppressHTMLCheck, mime, expected) {
  if (suppressHTMLCheck && mime == "text/html") {
    return null;
  }
  var data = SpecialPowers.wrap(getClipboardData(mime));
  is(
    data.value == null
      ? data.value
      : data.value.QueryInterface(SpecialPowers.Ci.nsISupportsString).data,
    expected,
    mime + " value in the clipboard"
  );
  return data.value;
}

function testSelectionToString(expected) {
  const flags =
    SpecialPowers.Ci.nsIDocumentEncoder.SkipInvisibleContent |
    SpecialPowers.Ci.nsIDocumentEncoder.AllowCrossShadowBoundary;
  is(
    SpecialPowers.wrap(window)
      .getSelection()
      .toStringWithFormat("text/plain", flags, 0)
      .replace(/\r\n/g, "\n"),
    expected,
    "Selection.toString"
  );
}

function testHtmlClipboardValue(suppressHTMLCheck, mime, expected) {
  // For Windows, navigator.platform returns "Win32".
  var expectedValue = expected;
  if (navigator.platform.includes("Win")) {
    // Windows has extra content.
    var expectedValue =
      kTextHtmlPrefixClipboardDataWindows +
      expected.replace(/\n/g, "\n") +
      kTextHtmlSuffixClipboardDataWindows;
  }
  testClipboardValue(suppressHTMLCheck, mime, expectedValue);
}

function testPasteText(textarea, expected) {
  textarea.value = "";
  textarea.focus();
  textarea.editor.paste(1);
  is(textarea.value, expected, "value of the textarea after the paste");
}

async function copySelectionToClipboard() {
  await SimpleTest.promiseClipboardChange(
    () => true,
    () => {
      documentViewer.copySelection();
    }
  );
  ok(clipboard.hasDataMatchingFlavors(["text/plain"], 1), "check text/plain");
  ok(clipboard.hasDataMatchingFlavors(["text/html"], 1), "check text/html");
}

async function testCopyPasteShadowDOM() {
  var textarea = SpecialPowers.wrap(document.getElementById("input"));

  function clear() {
    textarea.blur();
    var sel = window.getSelection();
    sel.removeAllRanges();
  }

  async function copySelectionToClipboardShadow(
    anchorNode,
    anchorOffset,
    focusNode,
    focusOffset
  ) {
    clear();
    var sel = window.getSelection();
    sel.setBaseAndExtent(anchorNode, anchorOffset, focusNode, focusOffset);
    await copySelectionToClipboard();
  }

  info(
    "Test 1: Both start and end are in light DOM, the range has contents in Shadow DOM."
  );
  await copySelectionToClipboardShadow(
    document.getElementById("title"),
    0,
    document.getElementById("host1"),
    1
  );
  testSelectionToString("This is a draggable bit of text.\nShadow Content1 ");
  testClipboardValue(
    false,
    "text/plain",
    "This is a draggable bit of text.\nShadow Content1 "
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '<div id="title" title="title to have a long HTML line">This is a <em>draggable</em> bit of text.</div>\n  <div id="host1">\n      <span id="shadow-content">Shadow Content1</span>\n    </div>'
  );
  testPasteText(textarea, "This is a draggable bit of text.\nShadow Content1 ");

  info("Test 2: Start is in Shadow DOM and end is in light DOM.");
  await copySelectionToClipboardShadow(
    document.getElementById("host1").shadowRoot.getElementById("shadow-content")
      .firstChild,
    3,
    document.getElementById("light-content").firstChild,
    5
  );
  testSelectionToString("dow Content1\nLight");
  testClipboardValue(false, "text/plain", "dow Content1\nLight");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<div id="host1"><span id="shadow-content">dow Content1</span>\n    </div>\n\n  <span id="light-content">Light</span>'
  );

  info("Test 3: Start is in light DOM and end is in shadow DOM.");
  await copySelectionToClipboardShadow(
    document.getElementById("light-content").firstChild,
    3,
    document.getElementById("host2").shadowRoot.getElementById("shadow-content")
      .firstChild,
    5
  );
  testSelectionToString("ht Content\nShado");
  testClipboardValue(false, "text/plain", "ht Content\nShado");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="light-content">ht Content</span>\n\n  <div id="host2">\n      <span id="shadow-content">Shado</span></div>'
  );

  info("Test 4: start is in light DOM and end is a nested shadow DOM.\n");
  await copySelectionToClipboardShadow(
    document.getElementById("light-content").firstChild,
    3,
    document
      .getElementById("host2")
      .shadowRoot.getElementById("nested-host")
      .shadowRoot.getElementById("nested-shadow-content").firstChild,
    5
  );
  testSelectionToString("ht Content\nShadow Content2\nNeste");
  testClipboardValue(false, "text/plain", "ht Content\nShadow Content2\nNeste");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="light-content">ht Content</span>\n\n  <div id="host2">\n      <span id="shadow-content">Shadow Content2</span>\n      <div id="nested-host">\n          <span id="nested-shadow-content">Neste</span></div></div>'
  );

  info("Test 5: Both start and end are in shadow DOM but in different trees.");
  await copySelectionToClipboardShadow(
    document.getElementById("host1").shadowRoot.getElementById("shadow-content")
      .firstChild,
    3,
    document
      .getElementById("host2")
      .shadowRoot.getElementById("nested-host")
      .shadowRoot.getElementById("nested-shadow-content").firstChild,
    5
  );
  testSelectionToString("dow Content1\nLight Content\nShadow Content2\nNeste");
  testClipboardValue(
    false,
    "text/plain",
    "dow Content1\nLight Content\nShadow Content2\nNeste"
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '<div id="host1"><span id="shadow-content">dow Content1</span>\n    </div>\n\n  <span id="light-content">Light Content</span>\n\n  <div id="host2">\n      <span id="shadow-content">Shadow Content2</span>\n      <div id="nested-host">\n          <span id="nested-shadow-content">Neste</span></div></div>'
  );

  info(
    "Test 6: Start is in a shadow tree and end is in a nested shadow tree within the same shadow tree."
  );
  await copySelectionToClipboardShadow(
    document.getElementById("host2").shadowRoot.getElementById("shadow-content")
      .firstChild,
    3,
    document
      .getElementById("host2")
      .shadowRoot.getElementById("nested-host")
      .shadowRoot.getElementById("nested-shadow-content").firstChild,
    5
  );
  testSelectionToString("dow Content2\nNeste");
  testClipboardValue(false, "text/plain", "dow Content2\nNeste");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="shadow-content">dow Content2</span>\n      <div id="nested-host">\n          <span id="nested-shadow-content">Neste</span></div>'
  );

  info(
    "Test 7: End is at a slotted content where the slot element is before the regular shadow dom contents."
  );
  await copySelectionToClipboardShadow(
    document.getElementById("light-content2").firstChild,
    3,
    document.getElementById("slotted1").firstChild,
    8
  );
  testSelectionToString("ht Content\nShadow Content2 slotted1");
  testClipboardValue(
    false,
    "text/plain",
    "ht Content\nShadow Content2 slotted1"
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="light-content2">ht Content</span>\n  <div id="host3">\n      <slot name="slot1"></slot>\n      <span id="shadow-content">Shadow Content2</span>\n      <slot name="slot2"></slot>\n    <span slot="slot1" id="slotted1">slotted1</span></div>'
  );

  info(
    "Test 8: End is at a slotted content where the slot element is after the regular shadow dom contents"
  );
  await copySelectionToClipboardShadow(
    document.getElementById("light-content2").firstChild,
    3,
    document.getElementById("slotted2").firstChild,
    8
  );
  testSelectionToString("ht Content\nShadow Content2 slotted1slotted2");
  testClipboardValue(
    false,
    "text/plain",
    "ht Content\nShadow Content2 slotted1slotted2"
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="light-content2">ht Content</span>\n  <div id="host3">\n      <slot name="slot1"></slot>\n      <span id="shadow-content">Shadow Content2</span>\n      <slot name="slot2"></slot>\n    <span slot="slot1" id="slotted1">slotted1</span><span slot="slot2" id="slotted2">slotted2</span></div>'
  );

  info(
    "Test 9: things still work as expected with a more complex shadow tree."
  );
  await copySelectionToClipboardShadow(
    document.getElementById("slotted3").firstChild,
    3,
    document.getElementById("slotted4").firstChild,
    8
  );
  testSelectionToString(
    " Shadow Content2\nShadowNested Nested Slotted\ntted1slotted2"
  );
  testClipboardValue(
    false,
    "text/plain",
    " Shadow Content2\nShadowNested Nested Slotted\ntted1slotted2"
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '\n      <slot name="slot1"></slot>\n      <span id="shadow-content">Shadow Content2</span>\n      <div id="nestedHost">\n          <slot></slot>\n          <span>ShadowNested</span>\n        \n        \n        <span>Nested Slotted</span>\n      </div>\n      <slot name="slot2"></slot>\n    <span slot="slot1" id="slotted3">tted1</span><span slot="slot2" id="slotted4">slotted2</span>'
  );

  // FIXME: This behaviour is not expected and we'll fix it in bug 1901053
  info("Test 10: Slot element is always serialized even if it's not visible");
  await copySelectionToClipboardShadow(
    document.getElementById("light-content3").firstChild,
    0,
    document.getElementById("host5").shadowRoot.querySelector("span")
      .firstChild,
    5
  );
  testSelectionToString("Light Content\ndefault value Shado Slotted ");
  testClipboardValue(
    false,
    "text/plain",
    "Light Content\ndefault value Shado Slotted "
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="light-content3">Light Content</span>\n  \n  <div id="host5">\n      <slot>default value</slot>\n      <span>Shado</span>\n    \n    <span>Slotted</span>\n  </div>'
  );
}

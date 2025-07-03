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

  info("Test 1: Start is in Light DOM and end is a slotted node.");
  await copySelectionToClipboardShadow(
    start1.firstChild,
    2,
    end1.firstChild,
    2
  );
  testSelectionToString("art\nEn");
  testClipboardValue(false, "text/plain", "art\nEn");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="start1">art</span>\n  <div id="host1">\n      <slot>\n    \n    <span id="end1">En</span></slot></div>'
  );
  testPasteText(textarea, "art\nEn");

  info(
    "Test 2: Start is in Light DOM and end is a slotted node, while there's a Shadow DOM node before the slotted node."
  );
  await copySelectionToClipboardShadow(
    start1.firstChild,
    2,
    host1.shadowRoot.getElementById("inner1").firstChild,
    3
  );
  testSelectionToString("art\nEnd Inn");
  testClipboardValue(false, "text/plain", "art\nEnd Inn");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="start1">art</span>\n  <div id="host1">\n      <slot>\n    \n    <span id="end1">End</span>\n  </slot>\n      <span id="inner1">Inn</span></div>'
  );
  testPasteText(textarea, "art\nEnd Inn");

  info("Test 3: Start is a slotted node and end is in shadow DOM.");
  await copySelectionToClipboardShadow(
    end1.firstChild,
    2,
    host1.shadowRoot.getElementById("inner1").firstChild,
    3
  );
  testSelectionToString("d Inn");
  testClipboardValue(false, "text/plain", "d Inn");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<slot><span id="end1">d</span>\n  </slot>\n      <span id="inner1">Inn</span>'
  );
  testPasteText(textarea, "d Inn");

  info(
    "Test 4: start is in light DOM and end is a slotted node with multiple assigned nodes in the same slot.\n"
  );
  await copySelectionToClipboardShadow(
    start2.firstChild,
    2,
    host2_slot2.firstChild,
    5
  );
  testSelectionToString("art\nSlotted1Slott");
  testClipboardValue(false, "text/plain", "art\nSlotted1Slott");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="start2">art</span>\n  <div id="host2">\n      <slot name="slot1"><span id="host2_slot1" slot="slot1">Slotted1</span><span id="host2_slot2" slot="slot1">Slott</span></slot></div>'
  );
  testPasteText(textarea, "art\nSlotted1Slott");

  info(
    "Test 5: start is in light DOM and end is a slotted node with endOffset includes the entire slotted node\n"
  );
  await copySelectionToClipboardShadow(
    start2.firstChild,
    2,
    host2_slot2.firstChild,
    8
  );
  testSelectionToString("art\nSlotted1Slotted2");
  testClipboardValue(false, "text/plain", "art\nSlotted1Slotted2");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="start2">art</span>\n  <div id="host2">\n      <slot name="slot1"><span id="host2_slot1" slot="slot1">Slotted1</span><span id="host2_slot2" slot="slot1">Slotted2</span></slot></div>'
  );
  testPasteText(textarea, "art\nSlotted1Slotted2");

  info("Test 6: start is in light DOM and end is a shadow node.\n");
  await copySelectionToClipboardShadow(
    start2.firstChild,
    2,
    host2.shadowRoot.getElementById("inner2").firstChild,
    3
  );
  testSelectionToString("art\nSlotted1Slotted2 Inn");
  testClipboardValue(false, "text/plain", "art\nSlotted1Slotted2 Inn");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="start2">art</span>\n  <div id="host2">\n      <slot name="slot1"><span id="host2_slot1" slot="slot1">Slotted1</span><span id="host2_slot2" slot="slot1">Slotted2</span></slot>\n      <span id="inner2">Inn</span></div>'
  );
  testPasteText(textarea, "art\nSlotted1Slotted2 Inn");

  info("Test 7: start is in light DOM and end is a slotted node.\n");
  await copySelectionToClipboardShadow(
    start2.firstChild,
    2,
    host2_slot4.firstChild,
    8
  );
  testSelectionToString("art\nSlotted1Slotted2 Inner Slotted3Slotted4");
  testClipboardValue(
    false,
    "text/plain",
    "art\nSlotted1Slotted2 Inner Slotted3Slotted4"
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '<span id="start2">art</span>\n  <div id="host2">\n      <slot name="slot1"><span id="host2_slot1" slot="slot1">Slotted1</span><span id="host2_slot2" slot="slot1">Slotted2</span></slot>\n      <span id="inner2">Inner</span>\n      <slot name="slot2"><span slot="slot2">Slotted3</span><span id="host2_slot4" slot="slot2">Slotted4</span></slot></div>'
  );
  testPasteText(textarea, "art\nSlotted1Slotted2 Inner Slotted3Slotted4");

  info(
    "Test 8: Both start and end are slotted nodes, and their DOM tree order is reversed compare to flat tree order.\n"
  );
  await copySelectionToClipboardShadow(
    host3_slot1.firstChild,
    2,
    host3_slot4.firstChild,
    8
  );
  testSelectionToString("otted1 Slotted2 Inner Slotted3 Slotted4");
  testClipboardValue(
    false,
    "text/plain",
    "otted1 Slotted2 Inner Slotted3 Slotted4"
  );
  testHtmlClipboardValue(
    false,
    "text/html",
    '<slot name="slot1"><span id="host3_slot1" slot="slot1">otted1</span></slot>\n      <slot name="slot2"><span id="host3_slot2" slot="slot2">Slotted2</span></slot>\n      <span id="inner2">Inner</span>\n      <slot name="slot3"><span id="host3_slot3" slot="slot3">Slotted3</span></slot>\n      <slot name="slot4"><span id="host3_slot4" slot="slot4">Slotted4</span></slot>'
  );
  testPasteText(textarea, "otted1 Slotted2 Inner Slotted3 Slotted4");

  info("Test 9: start is in Shadow DOM and end is in Light DOM.\n");
  await copySelectionToClipboardShadow(
    host3.shadowRoot.getElementById("inner2").firstChild,
    3,
    host3_slot1.firstChild,
    4
  );
  testSelectionToString("ted1 Slotted2 Inn");
  testClipboardValue(false, "text/plain", "ted1 Slotted2 Inn");
  testHtmlClipboardValue(
    false,
    "text/html",
    '<slot name="slot1"><span id="host3_slot1" slot="slot1">ted1</span></slot>\n      <slot name="slot2"><span id="host3_slot2" slot="slot2">Slotted2</span></slot>\n      <span id="inner2">Inn</span>'
  );
  testPasteText(textarea, "ted1 Slotted2 Inn");
}

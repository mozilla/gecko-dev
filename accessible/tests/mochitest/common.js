////////////////////////////////////////////////////////////////////////////////
// Interfaces

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;

const nsIAccessibleEvent = Components.interfaces.nsIAccessibleEvent;
const nsIAccessibleStateChangeEvent =
  Components.interfaces.nsIAccessibleStateChangeEvent;
const nsIAccessibleCaretMoveEvent =
  Components.interfaces.nsIAccessibleCaretMoveEvent;
const nsIAccessibleTextChangeEvent =
  Components.interfaces.nsIAccessibleTextChangeEvent;
const nsIAccessibleVirtualCursorChangeEvent =
  Components.interfaces.nsIAccessibleVirtualCursorChangeEvent;

const nsIAccessibleStates = Components.interfaces.nsIAccessibleStates;
const nsIAccessibleRole = Components.interfaces.nsIAccessibleRole;
const nsIAccessibleScrollType = Components.interfaces.nsIAccessibleScrollType;
const nsIAccessibleCoordinateType = Components.interfaces.nsIAccessibleCoordinateType;

const nsIAccessibleRelation = Components.interfaces.nsIAccessibleRelation;
const nsIAccessibleTextRange = Components.interfaces.nsIAccessibleTextRange;

const nsIAccessible = Components.interfaces.nsIAccessible;

const nsIAccessibleDocument = Components.interfaces.nsIAccessibleDocument;
const nsIAccessibleApplication = Components.interfaces.nsIAccessibleApplication;

const nsIAccessibleText = Components.interfaces.nsIAccessibleText;
const nsIAccessibleEditableText = Components.interfaces.nsIAccessibleEditableText;

const nsIAccessibleHyperLink = Components.interfaces.nsIAccessibleHyperLink;
const nsIAccessibleHyperText = Components.interfaces.nsIAccessibleHyperText;

const nsIAccessibleCursorable = Components.interfaces.nsIAccessibleCursorable;
const nsIAccessibleImage = Components.interfaces.nsIAccessibleImage;
const nsIAccessiblePivot = Components.interfaces.nsIAccessiblePivot;
const nsIAccessibleSelectable = Components.interfaces.nsIAccessibleSelectable;
const nsIAccessibleTable = Components.interfaces.nsIAccessibleTable;
const nsIAccessibleTableCell = Components.interfaces.nsIAccessibleTableCell;
const nsIAccessibleTraversalRule = Components.interfaces.nsIAccessibleTraversalRule;
const nsIAccessibleValue = Components.interfaces.nsIAccessibleValue;

const nsIObserverService = Components.interfaces.nsIObserverService;

const nsIDOMDocument = Components.interfaces.nsIDOMDocument;
const nsIDOMEvent = Components.interfaces.nsIDOMEvent;
const nsIDOMHTMLDocument = Components.interfaces.nsIDOMHTMLDocument;
const nsIDOMNode = Components.interfaces.nsIDOMNode;
const nsIDOMHTMLElement = Components.interfaces.nsIDOMHTMLElement;
const nsIDOMWindow = Components.interfaces.nsIDOMWindow;
const nsIDOMXULElement = Components.interfaces.nsIDOMXULElement;

const nsIPropertyElement = Components.interfaces.nsIPropertyElement;

////////////////////////////////////////////////////////////////////////////////
// OS detect

const MAC = (navigator.platform.indexOf("Mac") != -1);
const LINUX = (navigator.platform.indexOf("Linux") != -1);
const SOLARIS = (navigator.platform.indexOf("SunOS") != -1);
const WIN = (navigator.platform.indexOf("Win") != -1);

////////////////////////////////////////////////////////////////////////////////
// Application detect
// Firefox is assumed by default.

const SEAMONKEY = navigator.userAgent.match(/ SeaMonkey\//);

////////////////////////////////////////////////////////////////////////////////
// Accessible general

const STATE_BUSY = nsIAccessibleStates.STATE_BUSY;

const SCROLL_TYPE_ANYWHERE = nsIAccessibleScrollType.SCROLL_TYPE_ANYWHERE;

const COORDTYPE_SCREEN_RELATIVE = nsIAccessibleCoordinateType.COORDTYPE_SCREEN_RELATIVE;
const COORDTYPE_WINDOW_RELATIVE = nsIAccessibleCoordinateType.COORDTYPE_WINDOW_RELATIVE;
const COORDTYPE_PARENT_RELATIVE = nsIAccessibleCoordinateType.COORDTYPE_PARENT_RELATIVE;

const kEmbedChar = String.fromCharCode(0xfffc);

const kDiscBulletChar = String.fromCharCode(0x2022);
const kDiscBulletText = kDiscBulletChar + " ";
const kCircleBulletText = String.fromCharCode(0x25e6) + " ";
const kSquareBulletText = String.fromCharCode(0x25fe) + " ";

const MAX_TRIM_LENGTH = 100;

/**
 * nsIAccessibleRetrieval service.
 */
var gAccRetrieval = Components.classes["@mozilla.org/accessibleRetrieval;1"].
  getService(nsIAccessibleRetrieval);

/**
 * Enable/disable logging.
 */
function enableLogging(aModules)
{
  gAccRetrieval.setLogging(aModules);
}
function disableLogging()
{
  gAccRetrieval.setLogging("");
}
function isLogged(aModule)
{
  return gAccRetrieval.isLogged(aModule);
}

/**
 * Invokes the given function when document is loaded and focused. Preferable
 * to mochitests 'addLoadEvent' function -- additionally ensures state of the
 * document accessible is not busy.
 *
 * @param aFunc  the function to invoke
 */
function addA11yLoadEvent(aFunc, aWindow)
{
  function waitForDocLoad()
  {
    window.setTimeout(
      function()
      {
        var targetDocument = aWindow ? aWindow.document : document;
        var accDoc = getAccessible(targetDocument);
        var state = {};
        accDoc.getState(state, {});
        if (state.value & STATE_BUSY)
          return waitForDocLoad();

        window.setTimeout(aFunc, 0);
      },
      0
    );
  }

  SimpleTest.waitForFocus(waitForDocLoad, aWindow);
}

/**
 * Analogy of SimpleTest.is function used to compare objects.
 */
function isObject(aObj, aExpectedObj, aMsg)
{
  if (aObj == aExpectedObj) {
    ok(true, aMsg);
    return;
  }

  ok(false,
     aMsg + " - got '" + prettyName(aObj) +
            "', expected '" + prettyName(aExpectedObj) + "'");
}

////////////////////////////////////////////////////////////////////////////////
// Helpers for getting DOM node/accessible

/**
 * Return the DOM node by identifier (may be accessible, DOM node or ID).
 */
function getNode(aAccOrNodeOrID, aDocument)
{
  if (!aAccOrNodeOrID)
    return null;

  if (aAccOrNodeOrID instanceof nsIDOMNode)
    return aAccOrNodeOrID;

  if (aAccOrNodeOrID instanceof nsIAccessible)
    return aAccOrNodeOrID.DOMNode;

  node = (aDocument || document).getElementById(aAccOrNodeOrID);
  if (!node) {
    ok(false, "Can't get DOM element for " + aAccOrNodeOrID);
    return null;
  }

  return node;
}

/**
 * Constants indicates getAccessible doesn't fail if there is no accessible.
 */
const DONOTFAIL_IF_NO_ACC = 1;

/**
 * Constants indicates getAccessible won't fail if accessible doesn't implement
 * the requested interfaces.
 */
const DONOTFAIL_IF_NO_INTERFACE = 2;

/**
 * Return accessible for the given identifier (may be ID attribute or DOM
 * element or accessible object) or null.
 *
 * @param aAccOrElmOrID      [in] identifier to get an accessible implementing
 *                           the given interfaces
 * @param aInterfaces        [in, optional] the interface or an array interfaces
 *                           to query it/them from obtained accessible
 * @param aElmObj            [out, optional] object to store DOM element which
 *                           accessible is obtained for
 * @param aDoNotFailIf       [in, optional] no error for special cases (see
 *                            constants above)
 */
function getAccessible(aAccOrElmOrID, aInterfaces, aElmObj, aDoNotFailIf)
{
  if (!aAccOrElmOrID)
    return null;

  var elm = null;

  if (aAccOrElmOrID instanceof nsIAccessible) {
    elm = aAccOrElmOrID.DOMNode;

  } else if (aAccOrElmOrID instanceof nsIDOMNode) {
    elm = aAccOrElmOrID;

  } else {
    elm = document.getElementById(aAccOrElmOrID);
    if (!elm) {
      ok(false, "Can't get DOM element for " + aAccOrElmOrID);
      return null;
    }
  }

  if (aElmObj && (typeof aElmObj == "object"))
    aElmObj.value = elm;

  var acc = (aAccOrElmOrID instanceof nsIAccessible) ? aAccOrElmOrID : null;
  if (!acc) {
    try {
      acc = gAccRetrieval.getAccessibleFor(elm);
    } catch (e) {
    }

    if (!acc) {
      if (!(aDoNotFailIf & DONOTFAIL_IF_NO_ACC))
        ok(false, "Can't get accessible for " + aAccOrElmOrID);

      return null;
    }
  }

  if (!aInterfaces)
    return acc;

  if (!(aInterfaces instanceof Array))
    aInterfaces = [ aInterfaces ];

  for (var index = 0; index < aInterfaces.length; index++) {
    try {
      acc.QueryInterface(aInterfaces[index]);
    } catch (e) {
      if (!(aDoNotFailIf & DONOTFAIL_IF_NO_INTERFACE))
        ok(false, "Can't query " + aInterfaces[index] + " for " + aAccOrElmOrID);

      return null;
    }
  }

  return acc;
}

/**
 * Return true if the given identifier has an accessible, or exposes the wanted
 * interfaces.
 */
function isAccessible(aAccOrElmOrID, aInterfaces)
{
  return getAccessible(aAccOrElmOrID, aInterfaces, null,
                       DONOTFAIL_IF_NO_ACC | DONOTFAIL_IF_NO_INTERFACE) ?
    true : false;
}

/**
 * Return an accessible that contains the DOM node for the given identifier.
 */
function getContainerAccessible(aAccOrElmOrID)
{
  var node = getNode(aAccOrElmOrID);
  if (!node)
    return null;

  while ((node = node.parentNode) && !isAccessible(node));
  return node ? getAccessible(node) : null;
}

/**
 * Return root accessible for the given identifier.
 */
function getRootAccessible(aAccOrElmOrID)
{
  var acc = getAccessible(aAccOrElmOrID ? aAccOrElmOrID : document);
  return acc ? acc.rootDocument.QueryInterface(nsIAccessible) : null;
}

/**
 * Return tab document accessible the given accessible is contained by.
 */
function getTabDocAccessible(aAccOrElmOrID)
{
  var acc = getAccessible(aAccOrElmOrID ? aAccOrElmOrID : document);

  var docAcc = acc.document.QueryInterface(nsIAccessible);
  var containerDocAcc = docAcc.parent.document;

  // Test is running is stand-alone mode.
  if (acc.rootDocument == containerDocAcc)
    return docAcc;

  // In the case of running all tests together.
  return containerDocAcc.QueryInterface(nsIAccessible);
}

/**
 * Return application accessible.
 */
function getApplicationAccessible()
{
  return gAccRetrieval.getApplicationAccessible().
    QueryInterface(nsIAccessibleApplication);
}

/**
 * A version of accessible tree testing, doesn't fail if tree is not complete.
 */
function testElm(aID, aTreeObj)
{
  testAccessibleTree(aID, aTreeObj, kSkipTreeFullCheck);
}

/**
 * Flags used for testAccessibleTree
 */
const kSkipTreeFullCheck = 1;

/**
 * Compare expected and actual accessibles trees.
 *
 * @param  aAccOrElmOrID  [in] accessible identifier
 * @param  aAccTree       [in] JS object, each field corresponds to property of
 *                         accessible object. Additionally special properties
 *                         are presented:
 *                          children - an array of JS objects representing
 *                                      children of accessible
 *                          states   - an object having states and extraStates
 *                                      fields
 * @param aFlags          [in, optional] flags, see constants above
 */
function testAccessibleTree(aAccOrElmOrID, aAccTree, aFlags)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return;

  var accTree = aAccTree;

  // Support of simplified accessible tree object.
  var key = Object.keys(accTree)[0];
  var roleName = "ROLE_" + key;
  if (roleName in nsIAccessibleRole) {
    accTree = {
      role: nsIAccessibleRole[roleName],
      children: accTree[key]
    };
  }

  // Test accessible properties.
  for (var prop in accTree) {
    var msg = "Wrong value of property '" + prop + "' for " + prettyName(acc) + ".";

    switch (prop) {
    case "actions": {
      testActionNames(acc, accTree.actions);
      break;
    }

    case "attributes":
      testAttrs(acc, accTree[prop], true);
      break;

    case "absentAttributes":
      testAbsentAttrs(acc, accTree[prop]);
      break;

    case "interfaces": {
      var ifaces = (accTree[prop] instanceof Array) ?
        accTree[prop] : [ accTree[prop] ];
      for (var i = 0; i < ifaces.length; i++) {
        ok((acc instanceof ifaces[i]),
           "No " + ifaces[i] + " interface on " + prettyName(acc));
      }
      break;
    }

    case "relations": {
      for (var rel in accTree[prop])
        testRelation(acc, window[rel], accTree[prop][rel]);
      break;
    }

    case "role":
      isRole(acc, accTree[prop], msg);
      break;

    case "states":
    case "extraStates":
    case "absentStates":
    case "absentExtraStates": {
      testStates(acc, accTree["states"], accTree["extraStates"],
                 accTree["absentStates"], accTree["absentExtraStates"]);
      break;
    }

    case "tagName":
      is(accTree[prop], acc.DOMNode.tagName, msg);
      break;

    case "textAttrs": {
      var prevOffset = -1;
      for (var offset in accTree[prop]) {
        if (prevOffset !=- 1) {
          var attrs = accTree[prop][prevOffset];
          testTextAttrs(acc, prevOffset, attrs, { }, prevOffset, offset, true);
        }
        prevOffset = offset;
      }

      if (prevOffset != -1) {
        var charCount = getAccessible(acc, [nsIAccessibleText]).characterCount;
        var attrs = accTree[prop][prevOffset];
        testTextAttrs(acc, prevOffset, attrs, { }, prevOffset, charCount, true);
      }

      break;
    }

    default:
      if (prop.indexOf("todo_") == 0)
        todo(false, msg);
      else if (prop != "children")
        is(acc[prop], accTree[prop], msg);
    }
  }

  // Test children.
  if ("children" in accTree && accTree["children"] instanceof Array) {
    var children = acc.children;
    var childCount = children.length;

    is(childCount, accTree.children.length,
       "Different amount of expected children of " + prettyName(acc) + ".");

    if (accTree.children.length == childCount) {
      if (aFlags & kSkipTreeFullCheck) {
        for (var i = 0; i < childCount; i++) {
          var child = children.queryElementAt(i, nsIAccessible);
          testAccessibleTree(child, accTree.children[i], aFlags);
        }
        return;
      }

      // nsIAccessible::firstChild
      var expectedFirstChild = childCount > 0 ?
        children.queryElementAt(0, nsIAccessible) : null;
      var firstChild = null;
      try { firstChild = acc.firstChild; } catch (e) {}
      is(firstChild, expectedFirstChild,
         "Wrong first child of " + prettyName(acc));

      // nsIAccessible::lastChild
      var expectedLastChild = childCount > 0 ?
        children.queryElementAt(childCount - 1, nsIAccessible) : null;
      var lastChild = null;
      try { lastChild = acc.lastChild; } catch (e) {}
      is(lastChild, expectedLastChild,
         "Wrong last child of " + prettyName(acc));

      for (var i = 0; i < childCount; i++) {
        var child = children.queryElementAt(i, nsIAccessible);

        // nsIAccessible::parent
        var parent = null;
        try { parent = child.parent; } catch (e) {}
        is(parent, acc, "Wrong parent of " + prettyName(child));

        // nsIAccessible::indexInParent
        var indexInParent = -1;
        try { indexInParent = child.indexInParent; } catch(e) {}
        is(indexInParent, i,
           "Wrong index in parent of " + prettyName(child));

        // nsIAccessible::nextSibling
        var expectedNextSibling = (i < childCount - 1) ?
          children.queryElementAt(i + 1, nsIAccessible) : null;
        var nextSibling = null;
        try { nextSibling = child.nextSibling; } catch (e) {}
        is(nextSibling, expectedNextSibling,
           "Wrong next sibling of " + prettyName(child));

        // nsIAccessible::previousSibling
        var expectedPrevSibling = (i > 0) ?
          children.queryElementAt(i - 1, nsIAccessible) : null;
        var prevSibling = null;
        try { prevSibling = child.previousSibling; } catch (e) {}
        is(prevSibling, expectedPrevSibling,
           "Wrong previous sibling of " + prettyName(child));

        // Go down through subtree
        testAccessibleTree(child, accTree.children[i], aFlags);
      }
    }
  }
}

/**
 * Return true if accessible for the given node is in cache.
 */
function isAccessibleInCache(aNodeOrId)
{
  var node = getNode(aNodeOrId);
  return gAccRetrieval.getAccessibleFromCache(node) ? true : false;
}

/**
 * Test accessible tree for defunct accessible.
 *
 * @param  aAcc       [in] the defunct accessible
 * @param  aNodeOrId  [in] the DOM node identifier for the defunct accessible
 */
function testDefunctAccessible(aAcc, aNodeOrId)
{
  if (aNodeOrId)
    ok(!isAccessible(aNodeOrId),
       "Accessible for " + aNodeOrId + " wasn't properly shut down!");

  var msg = " doesn't fail for shut down accessible " + prettyName(aNodeOrId) + "!";

  // firstChild
  var success = false;
  try {
    aAcc.firstChild;
  } catch (e) {
    success = (e.result == Components.results.NS_ERROR_FAILURE)
  }
  ok(success, "firstChild" + msg);

  // lastChild
  success = false;
  try {
    aAcc.lastChild;
  } catch (e) {
    success = (e.result == Components.results.NS_ERROR_FAILURE)
  }
  ok(success, "lastChild" + msg);

  // childCount
  success = false;
  try {
    aAcc.childCount;
  } catch (e) {
    success = (e.result == Components.results.NS_ERROR_FAILURE)
  }
  ok(success, "childCount" + msg);

  // children
  success = false;
  try {
    aAcc.children;
  } catch (e) {
    success = (e.result == Components.results.NS_ERROR_FAILURE)
  }
  ok(success, "children" + msg);

  // nextSibling
  success = false;
  try {
    aAcc.nextSibling;
  } catch (e) {
    success = (e.result == Components.results.NS_ERROR_FAILURE);
  }
  ok(success, "nextSibling" + msg);

  // previousSibling
  success = false;
  try {
    aAcc.previousSibling;
  } catch (e) {
    success = (e.result == Components.results.NS_ERROR_FAILURE);
  }
  ok(success, "previousSibling" + msg);

  // parent
  success = false;
  try {
    aAcc.parent;
  } catch (e) {
    success = (e.result == Components.results.NS_ERROR_FAILURE);
  }
  ok(success, "parent" + msg);
}

/**
 * Convert role to human readable string.
 */
function roleToString(aRole)
{
  return gAccRetrieval.getStringRole(aRole);
}

/**
 * Convert states to human readable string.
 */
function statesToString(aStates, aExtraStates)
{
  var list = gAccRetrieval.getStringStates(aStates, aExtraStates);

  var str = "";
  for (var index = 0; index < list.length - 1; index++)
    str += list.item(index) + ", ";

  if (list.length != 0)
    str += list.item(index)

  return str;
}

/**
 * Convert event type to human readable string.
 */
function eventTypeToString(aEventType)
{
  return gAccRetrieval.getStringEventType(aEventType);
}

/**
 * Convert relation type to human readable string.
 */
function relationTypeToString(aRelationType)
{
  return gAccRetrieval.getStringRelationType(aRelationType);
}

function getLoadContext() {
  const Ci = Components.interfaces;
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsILoadContext);
}

/**
 * Return text from clipboard.
 */
function getTextFromClipboard()
{
  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].
    getService(Components.interfaces.nsIClipboard);
  if (!clip)
    return "";

  var trans = Components.classes["@mozilla.org/widget/transferable;1"].
    createInstance(Components.interfaces.nsITransferable);
  trans.init(getLoadContext());
  if (!trans)
    return "";

  trans.addDataFlavor("text/unicode");
  clip.getData(trans, clip.kGlobalClipboard);

  var str = new Object();
  var strLength = new Object();
  trans.getTransferData("text/unicode", str, strLength);

  if (str)
    str = str.value.QueryInterface(Components.interfaces.nsISupportsString);
  if (str)
    return str.data.substring(0, strLength.value / 2);

  return "";
}

/**
 * Return pretty name for identifier, it may be ID, DOM node or accessible.
 */
function prettyName(aIdentifier)
{
  if (aIdentifier instanceof Array) {
    var msg = "";
    for (var idx = 0; idx < aIdentifier.length; idx++) {
      if (msg != "")
        msg += ", ";

      msg += prettyName(aIdentifier[idx]);
    }
    return msg;
  }

  if (aIdentifier instanceof nsIAccessible) {
    var acc = getAccessible(aIdentifier);
    var msg = "[" + getNodePrettyName(acc.DOMNode);
    try {
      msg += ", role: " + roleToString(acc.role);
      if (acc.name)
        msg += ", name: '" + shortenString(acc.name) + "'";
    } catch (e) {
      msg += "defunct";
    }

    if (acc)
      msg += ", address: " + getObjAddress(acc);
    msg += "]";

    return msg;
  }

  if (aIdentifier instanceof nsIDOMNode)
    return "[ " + getNodePrettyName(aIdentifier) + " ]";

  return " '" + aIdentifier + "' ";
}

/**
 * Shorten a long string if it exceeds MAX_TRIM_LENGTH.
 * @param aString the string to shorten.
 * @returns the shortened string.
 */
function shortenString(aString, aMaxLength)
{
  if (aString.length <= MAX_TRIM_LENGTH)
    return aString;

  // Trim the string if its length is > MAX_TRIM_LENGTH characters.
  var trimOffset = MAX_TRIM_LENGTH / 2;
  return aString.substring(0, trimOffset - 1) + "..." +
    aString.substring(aString.length - trimOffset, aString.length);
}

////////////////////////////////////////////////////////////////////////////////
// General Utils
////////////////////////////////////////////////////////////////////////////////
/**
 * Return main chrome window (crosses chrome boundary)
 */
function getMainChromeWindow(aWindow)
{
  return aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIWebNavigation)
                .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
                .rootTreeItem
                .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIDOMWindow);
}

/** Sets the test plugin(s) initially expected enabled state.
 * It will automatically be reset to it's previous value after the test
 * ends.
 * @param aNewEnabledState [in] the enabled state, e.g. SpecialPowers.Ci.nsIPluginTag.STATE_ENABLED
 * @param aPluginName [in, optional] The name of the plugin, defaults to "Test Plug-in"
 */
function setTestPluginEnabledState(aNewEnabledState, aPluginName)
{
  var plugin = getTestPluginTag(aPluginName);
  var oldEnabledState = plugin.enabledState;
  plugin.enabledState = aNewEnabledState;
  SimpleTest.registerCleanupFunction(function() {
    getTestPluginTag(aPluginName).enabledState = oldEnabledState;
  });
}

////////////////////////////////////////////////////////////////////////////////
// Private
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Accessible general

function getNodePrettyName(aNode)
{
  try {
    var tag = "";
    if (aNode.nodeType == nsIDOMNode.DOCUMENT_NODE) {
      tag = "document";
    } else {
      tag = aNode.localName;
      if (aNode.nodeType == nsIDOMNode.ELEMENT_NODE && aNode.hasAttribute("id"))
        tag += "@id=\"" + aNode.getAttribute("id") + "\"";
    }

    return "'" + tag + " node', address: " + getObjAddress(aNode);
  } catch (e) {
    return "' no node info '";
  }
}

function getObjAddress(aObj)
{
  var exp = /native\s*@\s*(0x[a-f0-9]+)/g;
  var match = exp.exec(aObj.toString());
  if (match)
    return match[1];

  return aObj.toString();
}

function getTestPluginTag(aPluginName)
{
  var ph = SpecialPowers.Cc["@mozilla.org/plugin/host;1"]
                        .getService(SpecialPowers.Ci.nsIPluginHost);
  var tags = ph.getPluginTags();
  var name = aPluginName || "Test Plug-in";
  for (var tag of tags) {
    if (tag.name == name) {
      return tag;
    }
  }

  ok(false, "Could not find plugin tag with plugin name '" + name + "'");
  return null;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*** =================== SAVED SIGNONS CODE =================== ***/

var kSignonBundle;
var showingPasswords = false;
var dateFormatter = new Intl.DateTimeFormat(undefined,
                      { day: "numeric", month: "short", year: "numeric" });
var dateAndTimeFormatter = new Intl.DateTimeFormat(undefined,
                             { day: "numeric", month: "short", year: "numeric",
                               hour: "numeric", minute: "numeric" });

function SignonsStartup() {
  kSignonBundle = document.getElementById("signonBundle");
  document.getElementById("togglePasswords").label = kSignonBundle.getString("showPasswords");
  document.getElementById("togglePasswords").accessKey = kSignonBundle.getString("showPasswordsAccessKey");
  document.getElementById("signonsIntro").textContent = kSignonBundle.getString("loginsSpielAll");

  let treecols = document.getElementsByTagName("treecols")[0];
  treecols.addEventListener("click", HandleTreeColumnClick.bind(null, SignonColumnSort));

  LoadSignons();

  // filter the table if requested by caller
  if (window.arguments &&
      window.arguments[0] &&
      window.arguments[0].filterString) {
    setFilter(window.arguments[0].filterString);
    Services.telemetry.getHistogramById("PWMGR_MANAGE_OPENED").add(1);
  } else {
    Services.telemetry.getHistogramById("PWMGR_MANAGE_OPENED").add(0);
  }

  FocusFilterBox();
}

function setFilter(aFilterString) {
  document.getElementById("filter").value = aFilterString;
  _filterPasswords();
}

var signonsTreeView = {
  _filterSet : [],
  _lastSelectedRanges : [],
  selection: null,

  rowCount : 0,
  setTree : function(tree) {},
  getImageSrc : function(row,column) {},
  getProgressMode : function(row,column) {},
  getCellValue : function(row,column) {},
  getCellText : function(row,column) {
    var time;
    var signon = this._filterSet.length ? this._filterSet[row] : signons[row];
    switch (column.id) {
      case "siteCol":
        return signon.httpRealm ?
               (signon.hostname + " (" + signon.httpRealm + ")"):
               signon.hostname;
      case "userCol":
        return signon.username || "";
      case "passwordCol":
        return signon.password || "";
      case "timeCreatedCol":
        time = new Date(signon.timeCreated);
        return dateFormatter.format(time);
      case "timeLastUsedCol":
        time = new Date(signon.timeLastUsed);
        return dateAndTimeFormatter.format(time);
      case "timePasswordChangedCol":
        time = new Date(signon.timePasswordChanged);
        return dateFormatter.format(time);
      case "timesUsedCol":
        return signon.timesUsed;
      default:
        return "";
    }
  },
  isSeparator : function(index) { return false; },
  isSorted : function() { return false; },
  isContainer : function(index) { return false; },
  cycleHeader : function(column) {},
  getRowProperties : function(row) { return ""; },
  getColumnProperties : function(column) { return ""; },
  getCellProperties : function(row,column) {
    if (column.element.getAttribute("id") == "siteCol")
      return "ltr";

    return "";
  }
};


function LoadSignons() {
  // loads signons into table
  try {
    signons = passwordmanager.getAllLogins();
  } catch (e) {
    signons = [];
  }
  signons.forEach(login => login.QueryInterface(Components.interfaces.nsILoginMetaInfo));
  signonsTreeView.rowCount = signons.length;

  // sort and display the table
  signonsTree.view = signonsTreeView;
  // The sort column didn't change. SortTree (called by
  // SignonColumnSort) assumes we want to toggle the sort
  // direction but here we don't so we have to trick it
  lastSignonSortAscending = !lastSignonSortAscending;
  SignonColumnSort(lastSignonSortColumn);

  // disable "remove all signons" button if there are no signons
  var element = document.getElementById("removeAllSignons");
  var toggle = document.getElementById("togglePasswords");
  if (signons.length == 0) {
    element.setAttribute("disabled","true");
    toggle.setAttribute("disabled","true");
  } else {
    element.removeAttribute("disabled");
    toggle.removeAttribute("disabled");
  }

  return true;
}

function SignonSelected() {
  var selections = GetTreeSelections(signonsTree);
  if (selections.length) {
    document.getElementById("removeSignon").removeAttribute("disabled");
  }
}

function DeleteSignon() {
  var syncNeeded = (signonsTreeView._filterSet.length != 0);
  DeleteSelectedItemFromTree(signonsTree, signonsTreeView,
                             signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                             deletedSignons, "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions(syncNeeded);
}

function DeleteAllSignons() {
  var prompter = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                           .getService(Components.interfaces.nsIPromptService);

  // Confirm the user wants to remove all passwords
  var dummy = { value: false };
  if (prompter.confirmEx(window,
                         kSignonBundle.getString("removeAllPasswordsTitle"),
                         kSignonBundle.getString("removeAllPasswordsPrompt"),
                         prompter.STD_YES_NO_BUTTONS + prompter.BUTTON_POS_1_DEFAULT,
                         null, null, null, null, dummy) == 1) // 1 == "No" button
    return;

  var syncNeeded = (signonsTreeView._filterSet.length != 0);
  DeleteAllFromTree(signonsTree, signonsTreeView,
                        signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                        deletedSignons, "removeSignon", "removeAllSignons");
  FinalizeSignonDeletions(syncNeeded);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_DELETED_ALL").add(1);
}

function TogglePasswordVisible() {
  if (showingPasswords || masterPasswordLogin(AskUserShowPasswords)) {
    showingPasswords = !showingPasswords;
    document.getElementById("togglePasswords").label = kSignonBundle.getString(showingPasswords ? "hidePasswords" : "showPasswords");
    document.getElementById("togglePasswords").accessKey = kSignonBundle.getString(showingPasswords ? "hidePasswordsAccessKey" : "showPasswordsAccessKey");
    document.getElementById("passwordCol").hidden = !showingPasswords;
    _filterPasswords();
  }

  // Notify observers that the password visibility toggling is
  // completed.  (Mostly useful for tests)
  Components.classes["@mozilla.org/observer-service;1"]
            .getService(Components.interfaces.nsIObserverService)
            .notifyObservers(null, "passwordmgr-password-toggle-complete", null);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_VISIBILITY_TOGGLED").add(showingPasswords);
}

function AskUserShowPasswords() {
  var prompter = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  var dummy = { value: false };

  // Confirm the user wants to display passwords
  return prompter.confirmEx(window,
          null,
          kSignonBundle.getString("noMasterPasswordPrompt"), prompter.STD_YES_NO_BUTTONS,
          null, null, null, null, dummy) == 0;    // 0=="Yes" button
}

function FinalizeSignonDeletions(syncNeeded) {
  for (var s=0; s<deletedSignons.length; s++) {
    passwordmanager.removeLogin(deletedSignons[s]);
    Services.telemetry.getHistogramById("PWMGR_MANAGE_DELETED").add(1);
  }
  // If the deletion has been performed in a filtered view, reflect the deletion in the unfiltered table.
  // See bug 405389.
  if (syncNeeded) {
    try {
      signons = passwordmanager.getAllLogins();
    } catch (e) {
      signons = [];
    }
  }
  deletedSignons.length = 0;
}

function HandleSignonKeyPress(e) {
  if (e.keyCode == KeyEvent.DOM_VK_DELETE
#ifdef XP_MACOSX
      || e.keyCode == KeyEvent.DOM_VK_BACK_SPACE
#endif
     ) {
    DeleteSignon();
  }
}

function getColumnByName(column) {
  switch (column) {
    case "hostname":
      return document.getElementById("siteCol");
    case "username":
      return document.getElementById("userCol");
    case "password":
      return document.getElementById("passwordCol");
    case "timeCreated":
      return document.getElementById("timeCreatedCol");
    case "timeLastUsed":
      return document.getElementById("timeLastUsedCol");
    case "timePasswordChanged":
      return document.getElementById("timePasswordChangedCol");
    case "timesUsed":
      return document.getElementById("timesUsedCol");
  }
}

var lastSignonSortColumn = "hostname";
var lastSignonSortAscending = true;

function SignonColumnSort(column) {
  // clear out the sortDirection attribute on the old column
  var lastSortedCol = getColumnByName(lastSignonSortColumn);
  lastSortedCol.removeAttribute("sortDirection");

  // sort
  lastSignonSortAscending =
    SortTree(signonsTree, signonsTreeView,
                 signonsTreeView._filterSet.length ? signonsTreeView._filterSet : signons,
                 column, lastSignonSortColumn, lastSignonSortAscending);
  lastSignonSortColumn = column;

  // set the sortDirection attribute to get the styling going
  // first we need to get the right element
  var sortedCol = getColumnByName(column);
  sortedCol.setAttribute("sortDirection", lastSignonSortAscending ?
                                          "ascending" : "descending");
}

function SignonClearFilter() {
  var singleSelection = (signonsTreeView.selection.count == 1);

  // Clear the Tree Display
  signonsTreeView.rowCount = 0;
  signonsTree.treeBoxObject.rowCountChanged(0, -signonsTreeView._filterSet.length);
  signonsTreeView._filterSet = [];

  // Just reload the list to make sure deletions are respected
  LoadSignons();

  // Restore selection
  if (singleSelection) {
    signonsTreeView.selection.clearSelection();
    for (let i = 0; i < signonsTreeView._lastSelectedRanges.length; ++i) {
      var range = signonsTreeView._lastSelectedRanges[i];
      signonsTreeView.selection.rangedSelect(range.min, range.max, true);
    }
  } else {
    signonsTreeView.selection.select(0);
  }
  signonsTreeView._lastSelectedRanges = [];

  document.getElementById("signonsIntro").textContent = kSignonBundle.getString("loginsSpielAll");
}

function FocusFilterBox() {
  var filterBox = document.getElementById("filter");
  if (filterBox.getAttribute("focused") != "true")
    filterBox.focus();
}

function SignonMatchesFilter(aSignon, aFilterValue) {
  if (aSignon.hostname.toLowerCase().indexOf(aFilterValue) != -1)
    return true;
  if (aSignon.username &&
      aSignon.username.toLowerCase().indexOf(aFilterValue) != -1)
    return true;
  if (aSignon.httpRealm &&
      aSignon.httpRealm.toLowerCase().indexOf(aFilterValue) != -1)
    return true;
  if (showingPasswords && aSignon.password &&
      aSignon.password.toLowerCase().indexOf(aFilterValue) != -1)
    return true;

  return false;
}

function FilterPasswords(aFilterValue, view) {
  aFilterValue = aFilterValue.toLowerCase();
  return signons.filter(function (s) SignonMatchesFilter(s, aFilterValue));
}

function SignonSaveState() {
  // Save selection
  var seln = signonsTreeView.selection;
  signonsTreeView._lastSelectedRanges = [];
  var rangeCount = seln.getRangeCount();
  for (var i = 0; i < rangeCount; ++i) {
    var min = {}; var max = {};
    seln.getRangeAt(i, min, max);
    signonsTreeView._lastSelectedRanges.push({ min: min.value, max: max.value });
  }
}

function _filterPasswords()
{
  var filter = document.getElementById("filter").value;
  if (filter == "") {
    SignonClearFilter();
    return;
  }

  var newFilterSet = FilterPasswords(filter, signonsTreeView);
  if (!signonsTreeView._filterSet.length) {
    // Save Display Info for the Non-Filtered mode when we first
    // enter Filtered mode.
    SignonSaveState();
  }
  signonsTreeView._filterSet = newFilterSet;

  // Clear the display
  let oldRowCount = signonsTreeView.rowCount;
  signonsTreeView.rowCount = 0;
  signonsTree.treeBoxObject.rowCountChanged(0, -oldRowCount);
  // Set up the filtered display
  signonsTreeView.rowCount = signonsTreeView._filterSet.length;
  signonsTree.treeBoxObject.rowCountChanged(0, signonsTreeView.rowCount);

  // if the view is not empty then select the first item
  if (signonsTreeView.rowCount > 0)
    signonsTreeView.selection.select(0);

  document.getElementById("signonsIntro").textContent = kSignonBundle.getString("loginsSpielFiltered");
}

function CopyPassword() {
  // Don't copy passwords if we aren't already showing the passwords & a master
  // password hasn't been entered.
  if (!showingPasswords && !masterPasswordLogin())
    return;
  // Copy selected signon's password to clipboard
  var clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"].
                  getService(Components.interfaces.nsIClipboardHelper);
  var row = document.getElementById("signonsTree").currentIndex;
  var password = signonsTreeView.getCellText(row, {id : "passwordCol" });
  clipboard.copyString(password);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_COPIED_PASSWORD").add(1);
}

function CopyUsername() {
  // Copy selected signon's username to clipboard
  var clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"].
                  getService(Components.interfaces.nsIClipboardHelper);
  var row = document.getElementById("signonsTree").currentIndex;
  var username = signonsTreeView.getCellText(row, {id : "userCol" });
  clipboard.copyString(username);
  Services.telemetry.getHistogramById("PWMGR_MANAGE_COPIED_USERNAME").add(1);
}

function UpdateCopyPassword() {
  var singleSelection = (signonsTreeView.selection.count == 1);
  var passwordMenuitem = document.getElementById("context-copypassword");
  var usernameMenuitem = document.getElementById("context-copyusername");
  if (singleSelection) {
    usernameMenuitem.removeAttribute("disabled");
    passwordMenuitem.removeAttribute("disabled");
  } else {
    usernameMenuitem.setAttribute("disabled", "true");
    passwordMenuitem.setAttribute("disabled", "true");
  }
}

function masterPasswordLogin(noPasswordCallback) {
  // This doesn't harm if passwords are not encrypted
  var tokendb = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                    .createInstance(Components.interfaces.nsIPK11TokenDB);
  var token = tokendb.getInternalKeyToken();

  // If there is no master password, still give the user a chance to opt-out of displaying passwords
  if (token.checkPassword(""))
    return noPasswordCallback ? noPasswordCallback() : true;

  // So there's a master password. But since checkPassword didn't succeed, we're logged out (per nsIPK11Token.idl).
  try {
    // Relogin and ask for the master password.
    token.login(true);  // 'true' means always prompt for token password. User will be prompted until
                        // clicking 'Cancel' or entering the correct password.
  } catch (e) {
    // An exception will be thrown if the user cancels the login prompt dialog.
    // User is also logged out of Software Security Device.
  }

  return token.isLoggedIn();
}

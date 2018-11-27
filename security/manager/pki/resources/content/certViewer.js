/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from pippki.js */
"use strict";

/**
 * @file Implements functionality for certViewer.xul and its general and details
 *       tabs.
 * @argument {nsISupports} window.arguments[0]
 *           The cert to view, queryable to nsIX509Cert.
 */

const nsIX509Cert = Ci.nsIX509Cert;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Ci.nsIX509CertDB;
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Ci.nsIPK11TokenDB;
const nsIASN1Object = Ci.nsIASN1Object;
const nsIASN1Sequence = Ci.nsIASN1Sequence;
const nsIASN1PrintableItem = Ci.nsIASN1PrintableItem;
const nsIASN1Tree = Ci.nsIASN1Tree;
const nsASN1Tree = "@mozilla.org/security/nsASN1Tree;1";

var bundle;

/**
 * Fills out the "Certificate Hierarchy" tree of the cert viewer "Details" tab.
 *
 * @param {tree} node
 *        Parent tree node to append to.
 * @param {Array} chain
 *        An array of nsIX509Cert where cert n is issued by cert n + 1.
 */
function AddCertChain(node, chain) {
  if (!chain || chain.length < 1) {
    return;
  }
  let child = document.getElementById(node);
  // Clear any previous state.
  let preexistingChildren = child.querySelectorAll("treechildren");
  for (let preexistingChild of preexistingChildren) {
    child.removeChild(preexistingChild);
  }
  for (let i = chain.length - 1; i >= 0; i--) {
    let currCert = chain[i];
    let displayValue = currCert.displayName;
    let addTwistie = i != 0;
    child = addChildrenToTree(child, displayValue, currCert.dbKey, addTwistie);
  }
}

/**
 * Adds a "verified usage" of a cert to the "General" tab of the cert viewer.
 *
 * @param {String} usage
 *        Verified usage to add.
 */
function AddUsage(usage) {
  let verifyInfoBox = document.getElementById("verify_info_box");
  let text = document.createXULElement("textbox");
  text.setAttribute("value", usage);
  text.setAttribute("style", "margin: 2px 5px");
  text.setAttribute("readonly", "true");
  text.setAttribute("class", "scrollfield");
  verifyInfoBox.appendChild(text);
}

function setWindowName() {
  bundle = document.getElementById("pippki_bundle");

  let cert = window.arguments[0].QueryInterface(Ci.nsIX509Cert);
  document.title = bundle.getFormattedString("certViewerTitle",
                                             [cert.displayName]);

  //
  //  Set the cert attributes for viewing
  //

  // Set initial dummy chain of just the cert itself. A more complete chain (if
  // one can be found), will be set when the promise chain beginning at
  // asyncDetermineUsages finishes.
  AddCertChain("treesetDump", [cert]);
  DisplayGeneralDataFromCert(cert);
  BuildPrettyPrint(cert);

  asyncDetermineUsages(cert).then(displayUsages);
}

// Map of certificate usage name to localization identifier.
const certificateUsageToStringBundleName = {
  certificateUsageSSLClient: "VerifySSLClient",
  certificateUsageSSLServer: "VerifySSLServer",
  certificateUsageSSLCA: "VerifySSLCA",
  certificateUsageEmailSigner: "VerifyEmailSigner",
  certificateUsageEmailRecipient: "VerifyEmailRecip",
};

const SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const SEC_ERROR_EXPIRED_CERTIFICATE                     = SEC_ERROR_BASE + 11;
const SEC_ERROR_REVOKED_CERTIFICATE                     = SEC_ERROR_BASE + 12;
const SEC_ERROR_UNKNOWN_ISSUER                          = SEC_ERROR_BASE + 13;
const SEC_ERROR_UNTRUSTED_ISSUER                        = SEC_ERROR_BASE + 20;
const SEC_ERROR_UNTRUSTED_CERT                          = SEC_ERROR_BASE + 21;
const SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE              = SEC_ERROR_BASE + 30;
const SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED       = SEC_ERROR_BASE + 176;


/**
 * Updates the usage display area given the results from asyncDetermineUsages.
 *
 * @param {Array} results
 *        An array of objects with the properties "usageString", "errorCode",
 *        and "chain".
 *        usageString is a string that is a key in the certificateUsages map.
 *        errorCode is either an NSPR error code or PRErrorCodeSuccess (which is
 *        a pseudo-NSPR error code with the value 0 that indicates success).
 *        chain is the built trust path, if one was found
 */
function displayUsages(results) {
  document.getElementById("verify_pending").setAttribute("hidden", "true");
  let verified = document.getElementById("verified");
  let someSuccess = results.some(result =>
    result.errorCode == PRErrorCodeSuccess
  );
  if (someSuccess) {
    let verifystr = bundle.getString("certVerified");
    verified.textContent = verifystr;
    let pipnssBundle = Services.strings.createBundle(
      "chrome://pipnss/locale/pipnss.properties");
    results.forEach(result => {
      if (result.errorCode != PRErrorCodeSuccess) {
        return;
      }
      let bundleName = certificateUsageToStringBundleName[result.usageString];
      let usage = pipnssBundle.GetStringFromName(bundleName);
      AddUsage(usage);
    });
    AddCertChain("treesetDump", getBestChain(results));
  } else {
    const errorRankings = [
      { error: SEC_ERROR_REVOKED_CERTIFICATE,
        bundleString: "certNotVerified_CertRevoked" },
      { error: SEC_ERROR_UNTRUSTED_CERT,
        bundleString: "certNotVerified_CertNotTrusted" },
      { error: SEC_ERROR_UNTRUSTED_ISSUER,
        bundleString: "certNotVerified_IssuerNotTrusted" },
      { error: SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED,
        bundleString: "certNotVerified_AlgorithmDisabled" },
      { error: SEC_ERROR_EXPIRED_CERTIFICATE,
        bundleString: "certNotVerified_CertExpired" },
      { error: SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE,
        bundleString: "certNotVerified_CAInvalid" },
      { error: SEC_ERROR_UNKNOWN_ISSUER,
        bundleString: "certNotVerified_IssuerUnknown" },
    ];
    let verifystr;
    for (let errorRanking of errorRankings) {
      let errorPresent = results.some(result =>
        result.errorCode == errorRanking.error
      );
      if (errorPresent) {
        verifystr = bundle.getString(errorRanking.bundleString);
        break;
      }
    }
    if (!verifystr) {
      verifystr = bundle.getString("certNotVerified_Unknown");
    }
    verified.textContent = verifystr;
  }
  // Notify that we are done determining the certificate's valid usages (this
  // should be treated as an implementation detail that enables tests to run
  // efficiently - other code in the browser probably shouldn't rely on this).
  Services.obs.notifyObservers(window, "ViewCertDetails:CertUsagesDone");
}

function addChildrenToTree(parentTree, label, value, addTwistie) {
  let treeChild1 = document.createXULElement("treechildren");
  let treeElement = addTreeItemToTreeChild(treeChild1, label, value,
                                           addTwistie);
  parentTree.appendChild(treeChild1);
  return treeElement;
}

function addTreeItemToTreeChild(treeChild, label, value, addTwistie) {
  let treeElem1 = document.createXULElement("treeitem");
  if (addTwistie) {
    treeElem1.setAttribute("container", "true");
    treeElem1.setAttribute("open", "true");
  }
  let treeRow = document.createXULElement("treerow");
  let treeCell = document.createXULElement("treecell");
  treeCell.setAttribute("label", label);
  if (value) {
    treeCell.setAttribute("display", value);
  }
  treeRow.appendChild(treeCell);
  treeElem1.appendChild(treeRow);
  treeChild.appendChild(treeElem1);
  return treeElem1;
}

function displaySelected() {
  var asn1Tree = document.getElementById("prettyDumpTree")
          .view.QueryInterface(nsIASN1Tree);
  var items = asn1Tree.selection;
  var certDumpVal = document.getElementById("certDumpVal");
  if (items.currentIndex != -1) {
    var value = asn1Tree.getDisplayData(items.currentIndex);
    certDumpVal.value = value;
  } else {
    certDumpVal.value = "";
  }
}

function BuildPrettyPrint(cert) {
  var certDumpTree = Cc[nsASN1Tree].
                          createInstance(nsIASN1Tree);
  certDumpTree.loadASN1Structure(cert.ASN1Structure);
  document.getElementById("prettyDumpTree").view = certDumpTree;
}

function addAttributeFromCert(nodeName, value) {
  var node = document.getElementById(nodeName);
  if (!value) {
    value = bundle.getString("notPresent");
  }
  node.setAttribute("value", value);
}

/**
 * Displays information about a cert in the "General" tab of the cert viewer.
 *
 * @param {nsIX509Cert} cert
 *        Cert to display information about.
 */
function DisplayGeneralDataFromCert(cert) {
  addAttributeFromCert("commonname", cert.commonName);
  addAttributeFromCert("organization", cert.organization);
  addAttributeFromCert("orgunit", cert.organizationalUnit);
  addAttributeFromCert("serialnumber", cert.serialNumber);
  addAttributeFromCert("sha256fingerprint", cert.sha256Fingerprint);
  addAttributeFromCert("sha1fingerprint", cert.sha1Fingerprint);
  addAttributeFromCert("validitystart", cert.validity.notBeforeLocalDay);
  addAttributeFromCert("validityend", cert.validity.notAfterLocalDay);

  addAttributeFromCert("issuercommonname", cert.issuerCommonName);
  addAttributeFromCert("issuerorganization", cert.issuerOrganization);
  addAttributeFromCert("issuerorgunit", cert.issuerOrganizationUnit);
}

function updateCertDump() {
  var asn1Tree = document.getElementById("prettyDumpTree")
          .view.QueryInterface(nsIASN1Tree);

  var tree = document.getElementById("treesetDump");
  if (tree.currentIndex >= 0) {
    var item = tree.view.getItemAtIndex(tree.currentIndex);
    var dbKey = item.firstChild.firstChild.getAttribute("display");
    //  Get the cert from the cert database
    var certdb = Cc[nsX509CertDB].getService(nsIX509CertDB);
    var cert = certdb.findCertByDBKey(dbKey);
    asn1Tree.loadASN1Structure(cert.ASN1Structure);
  }
  displaySelected();
}

function getCurrentCert() {
  var realIndex;
  var tree = document.getElementById("treesetDump");
  if (tree.view.selection.isSelected(tree.currentIndex)
      && document.getElementById("prettyprint_tab").selected) {
    /* if the user manually selected a cert on the Details tab,
       then take that one  */
    realIndex = tree.currentIndex;
  } else {
    /* otherwise, take the one at the bottom of the chain
       (i.e. the one of the end-entity, unless we're displaying
       CA certs) */
    realIndex = tree.view.rowCount - 1;
  }
  if (realIndex >= 0) {
    var item = tree.view.getItemAtIndex(realIndex);
    var dbKey = item.firstChild.firstChild.getAttribute("display");
    var certdb = Cc[nsX509CertDB].getService(nsIX509CertDB);
    var cert = certdb.findCertByDBKey(dbKey);
    return cert;
  }
  /* shouldn't really happen */
  return null;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Error url MUST be formatted like this:
//   about:blocked?e=error_code&u=url(&o=1)?
//     (o=1 when user overrides are allowed)

// Note that this file uses document.documentURI to get
// the URL (with the format from above). This is because
// document.location.href gets the current URI off the docshell,
// which is the URL displayed in the location bar, i.e.
// the URI that the user attempted to load.

function getErrorCode() {
  var url = document.documentURI;
  var error = url.search(/e\=/);
  var duffUrl = url.search(/\&u\=/);
  return decodeURIComponent(url.slice(error + 2, duffUrl));
}

function getURL() {
  var url = document.documentURI;
  var match = url.match(/&u=([^&]+)&/);

  // match == null if not found; if so, return an empty string
  // instead of what would turn out to be portions of the URI
  if (!match) {
    return "";
  }

  url = decodeURIComponent(match[1]);

  // If this is a view-source page, then get then real URI of the page
  if (url.startsWith("view-source:")) {
    url = url.slice(12);
  }
  return url;
}

/**
 * Check whether this warning page is overridable or not, in which case
 * the "ignore the risk" suggestion in the error description
 * should not be shown.
 */
function getOverride() {
  var url = document.documentURI;
  var match = url.match(/&o=1&/);
  return !!match;
}

/**
 * Attempt to get the hostname via document.location.  Fail back
 * to getURL so that we always return something meaningful.
 */
function getHostString() {
  try {
    return document.location.hostname;
  } catch (e) {
    return getURL();
  }
}

function onClickSeeDetails() {
  let details = document.getElementById("errorDescriptionContainer");
  details.hidden = !details.hidden;
}

function initPage() {
  const errorMap = {
    malwareBlocked: "malware",
    deceptiveBlocked: "phishing",
    unwantedBlocked: "unwanted",
    harmfulBlocked: "harmful",
  };
  const error = errorMap[getErrorCode()];
  if (error === undefined) {
    return;
  }

  const messageIDs = {
    malware: {
      title: "safeb-blocked-malware-page-title",
      shortDesc: "safeb-blocked-malware-page-short-desc",
      innerDescOverride: "safeb-blocked-malware-page-error-desc-override-sumo",
      innerDescNoOverride:
        "safeb-blocked-malware-page-error-desc-no-override-sumo",
      learnMore: "safeb-blocked-malware-page-learn-more-sumo",
    },
    phishing: {
      title: "safeb-blocked-phishing-page-title",
      shortDesc: "safeb-blocked-phishing-page-short-desc",
      innerDescOverride: "safeb-blocked-phishing-page-error-desc-override",
      innerDescNoOverride: "safeb-blocked-phishing-page-error-desc-no-override",
      learnMore: "safeb-blocked-phishing-page-learn-more",
    },
    unwanted: {
      title: "safeb-blocked-unwanted-page-title",
      shortDesc: "safeb-blocked-unwanted-page-short-desc",
      innerDescOverride: "safeb-blocked-unwanted-page-error-desc-override",
      innerDescNoOverride: "safeb-blocked-unwanted-page-error-desc-no-override",
      learnMore: "safeb-blocked-unwanted-page-learn-more",
    },
    harmful: {
      title: "safeb-blocked-harmful-page-title",
      shortDesc: "safeb-blocked-harmful-page-short-desc",
      innerDescOverride: "safeb-blocked-harmful-page-error-desc-override",
      innerDescNoOverride: "safeb-blocked-harmful-page-error-desc-no-override",
      learnMore: "safeb-blocked-harmful-page-learn-more",
    },
  };

  // Set page contents depending on type of blocked page
  // Prepare the title and short description text
  let titleText = document.getElementById("errorTitleText");
  document.l10n.setAttributes(titleText, messageIDs[error].title);
  let shortDesc = document.getElementById("errorShortDescText");
  document.l10n.setAttributes(shortDesc, messageIDs[error].shortDesc);

  // Prepare the inner description, ensuring any redundant inner elements are removed.
  let innerDesc = document.getElementById("errorInnerDescription");
  let innerDescL10nID;
  if (!getOverride()) {
    innerDescL10nID = messageIDs[error].innerDescNoOverride;
    document.getElementById("ignore_warning_link").remove();
  } else {
    innerDescL10nID = messageIDs[error].innerDescOverride;
  }
  if (error == "unwanted" || error == "harmful") {
    document.getElementById("report_detection").remove();
  }

  // Add the inner description:
  document.l10n.setAttributes(innerDesc, innerDescL10nID, {
    sitename: getHostString(),
  });

  // Add the learn more content:
  let learnMore = document.getElementById("learn_more");
  document.l10n.setAttributes(learnMore, messageIDs[error].learnMore);

  // Set sitename to bold by adding class
  let errorSitename = document.getElementById("error_desc_sitename");
  errorSitename.setAttribute("class", "sitename");

  let titleEl = document.createElement("title");
  document.l10n.setAttributes(titleEl, messageIDs[error].title);
  document.head.appendChild(titleEl);

  // Inform the test harness that we're done loading the page.
  var event = new CustomEvent("AboutBlockedLoaded", {
    bubbles: true,
    detail: {
      url: this.getURL(),
      err: error,
    },
  });
  document.dispatchEvent(event);
}

let seeDetailsButton = document.getElementById("seeDetailsButton");
seeDetailsButton.addEventListener("click", onClickSeeDetails);
// Note: It is important to run the script this way, instead of using
// an onload handler. This is because error pages are loaded as
// LOAD_BACKGROUND, which means that onload handlers will not be executed.
initPage();

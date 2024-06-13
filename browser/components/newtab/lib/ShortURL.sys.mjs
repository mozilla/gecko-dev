/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on this module. This
// is because the Karma test environment already stubs out
// XPCOMUtils, and overrides importESModule to be a no-op (which
// can't be done for a static import statement).

// eslint-disable-next-line mozilla/use-static-import
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "IDNService",
  "@mozilla.org/network/idn-service;1",
  "nsIIDNService"
);

/**
 * Properly convert internationalized domain names.
 * @param {string} host Domain hostname.
 * @returns {string} Hostname suitable to be displayed.
 */
function handleIDNHost(hostname) {
  try {
    return lazy.IDNService.convertToDisplayIDN(hostname, {});
  } catch (e) {
    // If something goes wrong (e.g. host is an IP address) just fail back
    // to the full domain.
    return hostname;
  }
}

/**
 * Get the effective top level domain of a host.
 * @param {string} host The host to be analyzed.
 * @return {str} The suffix or empty string if there's no suffix.
 */
export function getETLD(host) {
  try {
    return Services.eTLD.getPublicSuffixFromHost(host);
  } catch (err) {
    return "";
  }
}

/**
 * shortHostname - Creates a short version of a hostname, used for display purposes
 *            e.g. "www.foosite.com"  =>  "foosite"
 *
 * @param {string} hostname The full hostname
 * @returns {string} The shortened hostname
 */
export function shortHostname(hostname) {
  if (!hostname) {
    return "";
  }

  const newHostname = hostname.replace(/^www\./i, "").toLowerCase();

  // Remove the eTLD (e.g., com, net) and the preceding period from the hostname
  const eTLD = getETLD(newHostname);
  const eTLDExtra =
    eTLD.length && newHostname.endsWith(eTLD) ? -(eTLD.length + 1) : Infinity;

  return handleIDNHost(newHostname.slice(0, eTLDExtra) || newHostname);
}

/**
 * shortURL - Creates a short version of a link's url, used for display purposes
 *            e.g. {url: http://www.foosite.com}  =>  "foosite"
 *
 * @param  {obj} link A link object
 *         {str} link.url (required)- The url of the link
 * @return {str}   A short url
 */
export function shortURL({ url }) {
  if (!url) {
    return "";
  }

  // Make sure we have a valid / parseable url
  let parsed;
  try {
    parsed = new URL(url);
  } catch (ex) {
    // Not entirely sure what we have, but just give it back
    return url;
  }

  // Ideally get the short eTLD-less host but fall back to longer url parts
  return shortHostname(parsed.hostname) || parsed.pathname || parsed.href;
}

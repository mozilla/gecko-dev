/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  ServiceWorkerCleanUp: "resource://gre/modules/ServiceWorkerCleanUp.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "sas",
  "@mozilla.org/storage/activity-service;1",
  "nsIStorageActivityService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "IdentityCredentialStorageService",
  "@mozilla.org/browser/identity-credential-storage-service;1",
  "nsIIdentityCredentialStorageService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "bounceTrackingProtection",
  "@mozilla.org/bounce-tracking-protection;1",
  "nsIBounceTrackingProtection"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "bounceTrackingProtectionMode",
  "privacy.bounceTrackingProtection.mode",
  Ci.nsIBounceTrackingProtection.MODE_DISABLED
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "permissionManagerIsolateByPrivateBrowsing",
  "permissions.isolateBy.privateBrowsing",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "permissionManagerIsolateByUserContext",
  "permissions.isolateBy.userContext",
  false
);

/**
 * Adds brackets to a host if it's an IPv6 address.
 * @param {string} host - Host which may be an IPv6.
 * @returns {string} bracketed IPv6 or host if host is not an IPv6.
 */
function maybeFixupIpv6(host) {
  if (!host?.includes(":")) {
    return host;
  }
  return `[${host}]`;
}

/**
 * Test if (host, OriginAttributes) or principal belong to a (schemeless) site.
 * Also considers partitioned storage by inspecting OriginAttributes
 * partitionKey.
 * @param options
 * @param {string} [options.host] - Optional host to compare to site.
 * @param {object} [options.originAttributes] - Optional origin attributes to
 * inspect for aSchemelessSite. If omitted, partitionKey and
 * aOriginAttributesPattern will not be matched.
 * @param {nsIPrincipal} [options.principal] - Optional principal to match with
 * aSchemelessSite and aOriginAttributesPattern.
 * @param {string} aSchemelessSite - Domain to check for. Must be a valid,
 * non-empty baseDomain string.
 * @param {Object} [aOriginAttributesPattern] - Additional OriginAttributes
 * filtering using an OriginAttributesPattern. Defaults to {} which matches all.
 * @returns {boolean} Whether the (host, originAttributes) or principal matches
 * the site.
 */
function hasSite(
  { host = null, originAttributes = null, principal = null },
  aSchemelessSite,
  aOriginAttributesPattern = {}
) {
  if (!aSchemelessSite) {
    throw new Error("Missing aSchemelessSite.");
  }
  if (!host && !originAttributes && !principal) {
    throw new Error(
      "Missing host, originAttributes or principal to match with aSchemelessSite."
    );
  }
  if (principal && (host || originAttributes)) {
    throw new Error(
      "Can only pass either principal or host and originAttributes."
    );
  }

  // If aSchemelessSite is an IPV6 host it will have brackets. Ensure that the
  // passed host has brackets too before comparing.
  host = maybeFixupIpv6(host);

  // If passed a host check if it belongs ot the given site.
  // originAttributes is optional. Only check for match if it's passed.
  if (
    host &&
    Services.eTLD.hasRootDomain(host, aSchemelessSite) &&
    (!originAttributes ||
      ChromeUtils.originAttributesMatchPattern(
        originAttributes,
        aOriginAttributesPattern
      ))
  ) {
    return true;
  }

  // If passed a principal check if it belongs to the given site. Also
  // check if the principal's OriginAttributes match our pattern.
  if (
    maybeFixupIpv6(principal?.baseDomain) == aSchemelessSite &&
    ChromeUtils.originAttributesMatchPattern(
      principal.originAttributes,
      aOriginAttributesPattern
    )
  ) {
    return true;
  }

  // Additionally check for partitioned state under the top level
  // aSchemelessSite. We need to inspect the OriginAttributes partitionKey for
  // that.
  let oa = originAttributes ?? principal?.originAttributes;
  if (oa == null) {
    // No OriginAttributes passed in to compare with.
    return false;
  }

  // For matching partitioned state under aSchemelessSite we use a
  // PartitionKeyPattern. Merge it with the aOriginAttributesPattern from the
  // caller.
  let patternWithPartitionKey = {
    ...aOriginAttributesPattern,
    partitionKeyPattern: { baseDomain: aSchemelessSite },
  };

  return ChromeUtils.originAttributesMatchPattern(oa, patternWithPartitionKey);
}

// Here is a list of methods cleaners may implement. These methods must return a
// Promise object.
// * deleteAll() - this method _must_ exist. When called, it deletes all the
//                 data owned by the cleaner.
// * deleteByPrincipal() -  this method _must_ exist.
// * deleteBySite() - this method _must_ exist.
// * deleteByHost() - this method is implemented only if the cleaner knows
//                    how to delete data by host + originAttributes pattern. If
//                    not implemented, deleteAll() will be used as fallback.
// * deleteByRange() - this method is implemented only if the cleaner knows how
//                    to delete data by time range. It receives 2 time range
//                    parameters: aFrom/aTo. If not implemented, deleteAll() is
//                    used as fallback.
// * deleteByLocalFiles() - this method removes data held for local files and
//                          other hostless origins. If not implemented,
//                          **no fallback is used**, as for a number of
//                          cleaners, no such data will ever exist and
//                          therefore clearing it does not make sense.
// * deleteByOriginAttributes() - this method is implemented only if the cleaner
//                                knows how to delete data by originAttributes
//                                pattern.
// * cleanupAfterDeletionAtShutdown() - this method is implemented only if the
//                                      cleaner needs a separate step after
//                                      deletion. No-op if not implemented.
//                                      Currently called via
//                                      Sanitizer.maybeSanitizeSessionPrincipals().

const CookieCleaner = {
  deleteByLocalFiles(aOriginAttributes) {
    return new Promise(aResolve => {
      Services.cookies.removeCookiesFromExactHost(
        "",
        JSON.stringify(aOriginAttributes)
      );
      aResolve();
    });
  },

  deleteByHost(aHost, aOriginAttributes) {
    return new Promise(aResolve => {
      Services.cookies.removeCookiesFromExactHost(
        aHost,
        JSON.stringify(aOriginAttributes)
      );
      aResolve();
    });
  },

  deleteByPrincipal(aPrincipal) {
    // Fall back to clearing by host and OA pattern. This will over-clear, since
    // any properties that are not explicitly set in aPrincipal.originAttributes
    // will be wildcard matched.
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    Services.cookies.cookies
      .filter(({ rawHost, originAttributes }) =>
        hasSite(
          { host: rawHost, originAttributes },
          aSchemelessSite,
          aOriginAttributesPattern
        )
      )
      .forEach(cookie => {
        Services.cookies.removeCookiesFromExactHost(
          cookie.rawHost,
          JSON.stringify(cookie.originAttributes)
        );
      });
  },

  deleteByRange(aFrom) {
    return Services.cookies.removeAllSince(aFrom);
  },

  deleteByOriginAttributes(aOriginAttributesString) {
    return new Promise(aResolve => {
      try {
        Services.cookies.removeCookiesWithOriginAttributes(
          aOriginAttributesString
        );
      } catch (ex) {}
      aResolve();
    });
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.cookies.removeAll();
      aResolve();
    });
  },
};

// A cleaner for clearing cookie banner handling exceptions.
const CookieBannerExceptionCleaner = {
  async deleteAll() {
    try {
      Services.cookieBanners.removeAllDomainPrefs(false);
    } catch (e) {
      // Don't throw an error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },

  async deleteByPrincipal(aPrincipal) {
    try {
      Services.cookieBanners.removeDomainPref(aPrincipal.URI, false);
    } catch (e) {
      // Don't throw an error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    let { privateBrowsingId } = aOriginAttributesPattern;

    try {
      let uri = Services.io.newURI("https://" + aSchemelessSite);

      // privateBrowsingId unset clears both normal and private browsing.
      // Otherwise only clear either normal or private browsing depending on the
      // value.
      if (
        privateBrowsingId == null ||
        privateBrowsingId ===
          Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID
      ) {
        Services.cookieBanners.removeDomainPref(uri, false);
      }
      if (
        privateBrowsingId == null ||
        privateBrowsingId !==
          Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID
      ) {
        Services.cookieBanners.removeDomainPref(uri, true);
      }
    } catch (e) {
      // Don't throw an error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },

  async deleteByHost(aHost, aOriginAttributes) {
    try {
      let isPrivate =
        !!aOriginAttributes.privateBrowsingId &&
        aOriginAttributes.privateBrowsingId !==
          Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID;

      Services.cookieBanners.removeDomainPref(
        Services.io.newURI("https://" + aHost),
        isPrivate
      );
    } catch (e) {
      // Don't throw an error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },
};

// A cleaner for cleaning cookie banner handling executed records.
const CookieBannerExecutedRecordCleaner = {
  async deleteAll() {
    try {
      Services.cookieBanners.removeAllExecutedRecords(false);
    } catch (e) {
      // Don't throw an error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },

  async deleteByPrincipal(aPrincipal) {
    try {
      Services.cookieBanners.removeExecutedRecordForSite(
        aPrincipal.baseDomain,
        false
      );
    } catch (e) {
      // Don't throw an error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    let { privateBrowsingId } = aOriginAttributesPattern;

    try {
      // privateBrowsingId unset clears both normal and private browsing.
      // Otherwise only clear either normal or private browsing depending on the
      // value
      if (
        privateBrowsingId == null ||
        privateBrowsingId ===
          Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID
      ) {
        Services.cookieBanners.removeExecutedRecordForSite(
          aSchemelessSite,
          false
        );
      }
      if (
        privateBrowsingId == null ||
        privateBrowsingId !==
          Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID
      ) {
        Services.cookieBanners.removeExecutedRecordForSite(
          aSchemelessSite,
          true
        );
      }
    } catch (e) {
      // Don't throw an error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },

  async deleteByHost(aHost, aOriginAttributes) {
    try {
      let isPrivate =
        !!aOriginAttributes.privateBrowsingId &&
        aOriginAttributes.privateBrowsingId !==
          Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID;

      Services.cookieBanners.removeExecutedRecordForSite(aHost, isPrivate);
    } catch (e) {
      // Don't throw error if the cookie banner handling is disabled.
      if (e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
        throw e;
      }
    }
  },
};

// A cleaner for cleaning fingerprinting protection states.
const FingerprintingProtectionStateCleaner = {
  async deleteAll() {
    Services.rfp.cleanAllRandomKeys();
  },

  async deleteByPrincipal(aPrincipal) {
    Services.rfp.cleanRandomKeyByPrincipal(aPrincipal);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    Services.rfp.cleanRandomKeyBySite(
      aSchemelessSite,
      aOriginAttributesPattern
    );
  },

  async deleteByHost(aHost, aOriginAttributesPattern) {
    Services.rfp.cleanRandomKeyByHost(
      aHost,
      JSON.stringify(aOriginAttributesPattern)
    );
  },

  async deleteByOriginAttributes(aOriginAttributesString) {
    Services.rfp.cleanRandomKeyByOriginAttributesPattern(
      aOriginAttributesString
    );
  },
};

const CertCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
      Ci.nsICertOverrideService
    );

    overrideService.clearValidityOverride(aHost, -1, aOriginAttributes);
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    let overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
      Ci.nsICertOverrideService
    );
    overrideService
      .getOverrides()
      .filter(({ asciiHost, originAttributes }) =>
        hasSite(
          { host: asciiHost, originAttributes },
          aSchemelessSite,
          aOriginAttributesPattern
        )
      )
      .forEach(({ asciiHost, port }) =>
        overrideService.clearValidityOverride(asciiHost, port, {})
      );
  },

  async deleteAll() {
    let overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(
      Ci.nsICertOverrideService
    );

    overrideService.clearAllOverrides();
  },
};

const NetworkCacheCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );

    Services.cache2.clearOrigin(httpPrincipal);
    Services.cache2.clearOrigin(httpsPrincipal);
  },

  async deleteBySite(aSchemelessSite, _aOriginAttributesPattern) {
    // TODO: aOriginAttributesPattern
    Services.cache2.clearBaseDomain(aSchemelessSite);
  },

  deleteByPrincipal(aPrincipal) {
    return new Promise(aResolve => {
      Services.cache2.clearOrigin(aPrincipal);
      aResolve();
    });
  },

  deleteByOriginAttributes(aOriginAttributesString) {
    return new Promise(aResolve => {
      Services.cache2.clearOriginAttributes(aOriginAttributesString);
      aResolve();
    });
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.cache2.clear();
      aResolve();
    });
  },
};

const CSSCacheCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );

    ChromeUtils.clearStyleSheetCacheByPrincipal(httpPrincipal);
    ChromeUtils.clearStyleSheetCacheByPrincipal(httpsPrincipal);
  },

  async deleteByPrincipal(aPrincipal) {
    ChromeUtils.clearStyleSheetCacheByPrincipal(aPrincipal);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    ChromeUtils.clearStyleSheetCacheBySite(
      aSchemelessSite,
      aOriginAttributesPattern
    );
  },

  async deleteAll() {
    ChromeUtils.clearStyleSheetCache();
  },
};

const MessagingLayerSecurityStateCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );
    ChromeUtils.clearMessagingLayerSecurityStateByPrincipal(httpsPrincipal);
    // The WebAPI doesn't allow for non-secure contexts but
    // we are keeping this out of caution.
    ChromeUtils.clearMessagingLayerSecurityStateByPrincipal(httpPrincipal);
  },
  async deleteByPrincipal(aPrincipal) {
    ChromeUtils.clearMessagingLayerSecurityStateByPrincipal(aPrincipal);
  },
  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    ChromeUtils.clearMessagingLayerSecurityStateBySite(
      aSchemelessSite,
      aOriginAttributesPattern
    );
  },
  async deleteAll() {
    ChromeUtils.clearMessagingLayerSecurityState();
  },
};

const JSCacheCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );

    ChromeUtils.clearScriptCacheByPrincipal(httpPrincipal);
    ChromeUtils.clearScriptCacheByPrincipal(httpsPrincipal);
  },

  async deleteByPrincipal(aPrincipal) {
    ChromeUtils.clearScriptCacheByPrincipal(aPrincipal);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    ChromeUtils.clearScriptCacheBySite(
      aSchemelessSite,
      aOriginAttributesPattern
    );
  },

  async deleteAll() {
    ChromeUtils.clearScriptCache();
  },
};

const ImageCacheCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);

    // Delete data from both HTTP and HTTPS sites.
    let httpURI = Services.io.newURI("http://" + aHost);
    let httpsURI = Services.io.newURI("https://" + aHost);
    let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpURI,
      aOriginAttributes
    );
    let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      httpsURI,
      aOriginAttributes
    );

    imageCache.removeEntriesFromPrincipalInAllProcesses(httpPrincipal);
    imageCache.removeEntriesFromPrincipalInAllProcesses(httpsPrincipal);
  },

  async deleteByPrincipal(aPrincipal) {
    let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);
    imageCache.removeEntriesFromPrincipalInAllProcesses(aPrincipal);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    let imageCache = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .getImgCacheForDocument(null);
    imageCache.removeEntriesFromSiteInAllProcesses(
      aSchemelessSite,
      aOriginAttributesPattern
    );
  },

  deleteAll() {
    return new Promise(aResolve => {
      let imageCache = Cc["@mozilla.org/image/tools;1"]
        .getService(Ci.imgITools)
        .getImgCacheForDocument(null);
      imageCache.clearCache(false); // true=chrome, false=content
      aResolve();
    });
  },
};

const DownloadsCleaner = {
  async _deleteInternal({ host, principal, originAttributes }) {
    originAttributes = originAttributes || principal?.originAttributes || {};

    let list = await lazy.Downloads.getList(lazy.Downloads.ALL);
    list.removeFinished(({ source }) => {
      if (
        "userContextId" in originAttributes &&
        "userContextId" in source &&
        originAttributes.userContextId != source.userContextId
      ) {
        return false;
      }
      if (
        "privateBrowsingId" in originAttributes &&
        !!originAttributes.privateBrowsingId != source.isPrivate
      ) {
        return false;
      }

      let entryURI = Services.io.newURI(source.url);
      if (host) {
        return Services.eTLD.hasRootDomain(entryURI.host, host);
      }
      if (principal) {
        return principal.equalsURI(entryURI);
      }
      return false;
    });
  },

  async deleteByHost(aHost, aOriginAttributes) {
    // Clearing by host also clears associated subdomains.
    return this._deleteInternal({
      host: aHost,
      originAttributes: aOriginAttributes,
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this._deleteInternal({ principal: aPrincipal });
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    let list = await lazy.Downloads.getList(lazy.Downloads.ALL);
    list.removeFinished(({ source }) => {
      if (
        "userContextId" in aOriginAttributesPattern &&
        "userContextId" in source &&
        aOriginAttributesPattern.userContextId != source.userContextId
      ) {
        return false;
      }
      if (
        "privateBrowsingId" in aOriginAttributesPattern &&
        !!aOriginAttributesPattern.privateBrowsingId != source.isPrivate
      ) {
        return false;
      }

      let entryURI = Services.io.newURI(source.url);
      return Services.eTLD.getSchemelessSite(entryURI) == aSchemelessSite;
    });
  },

  deleteByRange(aFrom, aTo) {
    // Convert microseconds back to milliseconds for date comparisons.
    let rangeBeginMs = aFrom / 1000;
    let rangeEndMs = aTo / 1000;

    return lazy.Downloads.getList(lazy.Downloads.ALL).then(aList => {
      aList.removeFinished(
        aDownload =>
          aDownload.startTime >= rangeBeginMs &&
          aDownload.startTime <= rangeEndMs
      );
    });
  },

  deleteAll() {
    return lazy.Downloads.getList(lazy.Downloads.ALL).then(aList => {
      aList.removeFinished(null);
    });
  },
};

const MediaDevicesCleaner = {
  async deleteByRange(aFrom) {
    let mediaMgr = Cc["@mozilla.org/mediaManagerService;1"].getService(
      Ci.nsIMediaManagerService
    );
    mediaMgr.sanitizeDeviceIds(aFrom);
  },

  // TODO: We should call the MediaManager to clear by principal, rather than
  // over-clearing for user requests or bailing out for programmatic calls.
  async deleteByPrincipal(aPrincipal, aIsUserRequest) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  // TODO: Same as above, but for site.
  async deleteBySite(
    _aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  async deleteAll() {
    let mediaMgr = Cc["@mozilla.org/mediaManagerService;1"].getService(
      Ci.nsIMediaManagerService
    );
    mediaMgr.sanitizeDeviceIds(null);
  },
};

const QuotaCleaner = {
  /**
   * Clear quota storage for matching principals.
   * @param {function} filterFn - Filter function which is passed a principal.
   * Return true to clear storage for given principal or false to skip it.
   * @returns {Promise} - Resolves once all matching items have been cleared.
   * Rejects on error.
   */
  async _qmsClearStoragesForPrincipalsMatching(filterFn) {
    // Clearing quota storage by first getting all entry origins and then
    // iterating over them is not ideal, since we can not ensure an entirely
    // consistent clearing state. Between fetching the origins and clearing
    // them, additional entries could be added. This means we could end up with
    // stray entries after the clearing operation. To fix this we would need to
    // move the clearing code to the QuotaManager itself which could either
    // prevent new writes while clearing or clean up any additional entries
    // which get written during the clearing operation.
    // Performance is also not ideal, since we iterate over storage multiple
    // times for this two step process.
    // See Bug 1719195.
    let origins = await new Promise((resolve, reject) => {
      Services.qms.listOrigins().callback = request => {
        if (request.resultCode != Cr.NS_OK) {
          reject({ message: "Deleting quota storages failed" });
          return;
        }
        resolve(request.result);
      };
    });

    let clearPromises = origins
      // Parse origins into principals.
      .map(Services.scriptSecurityManager.createContentPrincipalFromOrigin)
      // Filter out principals that don't match the filterFn.
      .filter(filterFn)
      // Clear quota storage by principal and collect the promises.
      .map(
        principal =>
          new Promise((resolve, reject) => {
            let clearRequest =
              Services.qms.clearStoragesForPrincipal(principal);
            clearRequest.callback = () => {
              if (clearRequest.resultCode != Cr.NS_OK) {
                reject({ message: "Deleting quota storages failed" });
                return;
              }
              resolve();
            };
          })
      );
    return Promise.all(clearPromises);
  },

  deleteByPrincipal(aPrincipal) {
    // localStorage: The legacy LocalStorage implementation that will
    // eventually be removed depends on this observer notification to clear by
    // principal.
    Services.obs.notifyObservers(
      null,
      "extension:purge-localStorage",
      aPrincipal.host
    );

    // Clear sessionStorage
    Services.sessionStorage.clearStoragesForOrigin(aPrincipal);

    // ServiceWorkers: they must be removed before cleaning QuotaManager.
    return lazy.ServiceWorkerCleanUp.removeFromPrincipal(aPrincipal)
      .then(
        _ => /* exceptionThrown = */ false,
        _ => /* exceptionThrown = */ true
      )
      .then(exceptionThrown => {
        // QuotaManager: In the event of a failure, we call reject to propagate
        // the error upwards.
        return new Promise((aResolve, aReject) => {
          let req = Services.qms.clearStoragesForPrincipal(aPrincipal);
          req.callback = () => {
            if (exceptionThrown || req.resultCode != Cr.NS_OK) {
              aReject({ message: "Delete by principal failed" });
            } else {
              aResolve();
            }
          };
        });
      });
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    // localStorage: The legacy LocalStorage implementation that will
    // eventually be removed depends on this observer notification to clear by
    // host.  Some other subsystems like Reporting headers depend on this too.
    // TODO: aOriginAttributesPattern
    Services.obs.notifyObservers(
      null,
      "extension:purge-localStorage",
      aSchemelessSite
    );

    // Clear sessionStorage
    // TODO: aOriginAttributesPattern
    Services.obs.notifyObservers(
      null,
      "browser:purge-sessionStorage",
      aSchemelessSite
    );

    // Clear third-party storage partitioned under aSchemelessSite.
    // This notification is forwarded via the StorageObserver and consumed only
    // by the SessionStorageManager and (legacy) LocalStorageManager.
    // There is a similar (legacy) notification "clear-origin-attributes-data"
    // which additionally clears data across various other storages unrelated to
    // the QuotaCleaner.
    Services.obs.notifyObservers(
      null,
      "dom-storage:clear-origin-attributes-data",
      JSON.stringify({
        ...aOriginAttributesPattern,
        partitionKeyPattern: { baseDomain: aSchemelessSite },
      })
    );

    // ServiceWorkers must be removed before cleaning QuotaManager. We store
    // potential errors so we can re-throw later, once all operations have
    // completed.
    let swCleanupError;
    try {
      await lazy.ServiceWorkerCleanUp.removeFromSite(
        aSchemelessSite,
        aOriginAttributesPattern
      );
    } catch (error) {
      swCleanupError = error;
    }

    await this._qmsClearStoragesForPrincipalsMatching(principal =>
      hasSite({ principal }, aSchemelessSite, aOriginAttributesPattern)
    );

    // Re-throw any service worker cleanup errors.
    if (swCleanupError) {
      throw swCleanupError;
    }
  },

  async deleteByHost(aHost) {
    // XXX: The aOriginAttributes is expected to always be empty({}). Maybe have
    // a debug assertion here to ensure that?

    // localStorage: The legacy LocalStorage implementation that will
    // eventually be removed depends on this observer notification to clear by
    // host.  Some other subsystems like Reporting headers depend on this too.
    Services.obs.notifyObservers(null, "extension:purge-localStorage", aHost);

    // Clear sessionStorage
    Services.obs.notifyObservers(null, "browser:purge-sessionStorage", aHost);

    // ServiceWorkers must be removed before cleaning QuotaManager. We store any
    // errors so we can re-throw later once all operations have completed.
    let swCleanupError;
    try {
      await lazy.ServiceWorkerCleanUp.removeFromHost(aHost);
    } catch (error) {
      swCleanupError = error;
    }

    await this._qmsClearStoragesForPrincipalsMatching(principal => {
      try {
        // deleteByHost has the semantics that "foo.example.com" should be
        // wiped if we are provided an aHost of "example.com".
        return Services.eTLD.hasRootDomain(principal.host, aHost);
      } catch (e) {
        // There is no host for the given principal.
        return false;
      }
    });

    // Re-throw any service worker cleanup errors.
    if (swCleanupError) {
      throw swCleanupError;
    }
  },

  deleteByRange(aFrom, aTo) {
    let principals = lazy.sas
      .getActiveOrigins(aFrom, aTo)
      .QueryInterface(Ci.nsIArray);

    let promises = [];
    for (let i = 0; i < principals.length; ++i) {
      let principal = principals.queryElementAt(i, Ci.nsIPrincipal);

      if (
        !principal.schemeIs("http") &&
        !principal.schemeIs("https") &&
        !principal.schemeIs("file")
      ) {
        continue;
      }

      promises.push(this.deleteByPrincipal(principal));
    }

    return Promise.all(promises);
  },

  deleteByOriginAttributes(aOriginAttributesString) {
    // The legacy LocalStorage implementation that will eventually be removed.
    // And it should've been cleared while notifying observers with
    // clear-origin-attributes-data.

    return lazy.ServiceWorkerCleanUp.removeFromOriginAttributes(
      aOriginAttributesString
    )
      .then(
        _ => /* exceptionThrown = */ false,
        _ => /* exceptionThrown = */ true
      )
      .then(() => {
        // QuotaManager: In the event of a failure, we call reject to propagate
        // the error upwards.
        return new Promise((aResolve, aReject) => {
          let req = Services.qms.clearStoragesForOriginAttributesPattern(
            aOriginAttributesString
          );
          req.callback = () => {
            if (req.resultCode == Cr.NS_OK) {
              aResolve();
            } else {
              aReject({ message: "Delete by origin attributes failed" });
            }
          };
        });
      });
  },

  async deleteAll() {
    // localStorage
    Services.obs.notifyObservers(null, "extension:purge-localStorage");

    // sessionStorage
    Services.obs.notifyObservers(null, "browser:purge-sessionStorage");

    // ServiceWorkers must be removed before cleaning QuotaManager. We store any
    // errors so we can re-throw later once all operations have completed.
    let swCleanupError;
    try {
      await lazy.ServiceWorkerCleanUp.removeAll();
    } catch (error) {
      swCleanupError = error;
    }

    await this._qmsClearStoragesForPrincipalsMatching(
      principal =>
        principal.schemeIs("http") ||
        principal.schemeIs("https") ||
        principal.schemeIs("file")
    );

    // Re-throw any service worker cleanup errors.
    if (swCleanupError) {
      throw swCleanupError;
    }
  },

  async cleanupAfterDeletionAtShutdown() {
    const toBeRemovedDir = PathUtils.join(
      PathUtils.profileDir,
      Services.prefs.getStringPref("dom.quotaManager.storageName"),
      "to-be-removed"
    );

    if (
      !AppConstants.MOZ_BACKGROUNDTASKS ||
      !Services.prefs.getBoolPref("dom.quotaManager.backgroundTask.enabled")
    ) {
      await IOUtils.remove(toBeRemovedDir, { recursive: true });
      return;
    }

    const runner = Cc["@mozilla.org/backgroundtasksrunner;1"].getService(
      Ci.nsIBackgroundTasksRunner
    );

    runner.removeDirectoryInDetachedProcess(
      toBeRemovedDir,
      "",
      "0",
      "*", // wildcard
      "Quota"
    );
  },
};

const PredictorNetworkCleaner = {
  async deleteAll() {
    // Predictive network data - like cache, no way to clear this per
    // domain, so just trash it all
    let np = Cc["@mozilla.org/network/predictor;1"].getService(
      Ci.nsINetworkPredictor
    );
    np.reset();
  },

  // TODO: We should call the NetworkPredictor to clear by principal, rather
  // than over-clearing for user requests or bailing out for programmatic calls.
  async deleteByPrincipal(aPrincipal, aIsUserRequest) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  // TODO: Same as above, but for base domain.
  async deleteBySite(
    _aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },
};

const PushNotificationsCleaner = {
  /**
   * Clear entries for aDomain including subdomains of aDomain.
   * @param {string} aDomain - Domain to clear data for.
   * @param {Object} aOriginAttributesPattern - Optional pattern to filter OriginAttributes.
   * @returns {Promise} a promise which resolves once data has been cleared.
   */
  _deleteByRootDomain(aDomain, aOriginAttributesPattern = null) {
    if (!Services.prefs.getBoolPref("dom.push.enabled", false)) {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      let push = Cc["@mozilla.org/push/Service;1"].getService(
        Ci.nsIPushService
      );
      // ClearForDomain also clears subdomains.
      push.clearForDomain(aDomain, aOriginAttributesPattern, aStatus => {
        if (!Components.isSuccessCode(aStatus)) {
          aReject();
        } else {
          aResolve();
        }
      });
    });
  },

  deleteByHost(aHost) {
    // Will also clear entries for subdomains of aHost. Data is cleared across
    // all origin attributes.
    return this._deleteByRootDomain(aHost);
  },

  deleteByPrincipal(aPrincipal) {
    if (!Services.prefs.getBoolPref("dom.push.enabled", false)) {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      let push = Cc["@mozilla.org/push/Service;1"].getService(
        Ci.nsIPushService
      );
      push.clearForPrincipal(aPrincipal, aStatus => {
        if (!Components.isSuccessCode(aStatus)) {
          aReject();
        } else {
          aResolve();
        }
      });
    });
  },

  deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    return this._deleteByRootDomain(aSchemelessSite, aOriginAttributesPattern);
  },

  deleteAll() {
    if (!Services.prefs.getBoolPref("dom.push.enabled", false)) {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      let push = Cc["@mozilla.org/push/Service;1"].getService(
        Ci.nsIPushService
      );
      push.clearForDomain("*", null, aStatus => {
        if (!Components.isSuccessCode(aStatus)) {
          aReject();
        } else {
          aResolve();
        }
      });
    });
  },
};

const StorageAccessCleaner = {
  // This is a special function to implement deleteUserInteractionForClearingHistory.
  async deleteExceptPrincipals(aPrincipalsWithStorage, aFrom) {
    // We compare by base domain in order to simulate the behavior
    // from purging, Consider a scenario where the user is logged
    // into sub.example.com but the cookies are on example.com. In this
    // case, we will remove the user interaction for sub.example.com
    // because its principal does not match the one with storage.
    let baseDomainsWithStorage = new Set();
    for (let principal of aPrincipalsWithStorage) {
      baseDomainsWithStorage.add(principal.baseDomain);
    }
    for (let perm of Services.perms.getAllByTypeSince(
      "storageAccessAPI",
      // The permission manager uses milliseconds instead of microseconds
      aFrom / 1000
    )) {
      if (!baseDomainsWithStorage.has(perm.principal.baseDomain)) {
        Services.perms.removePermission(perm);
      }
    }
  },

  async deleteByPrincipal(aPrincipal) {
    return Services.perms.removeFromPrincipal(aPrincipal, "storageAccessAPI");
  },

  _deleteInternal(filter) {
    Services.perms.all
      .filter(({ type }) => type == "storageAccessAPI")
      .filter(filter)
      .forEach(perm => {
        try {
          Services.perms.removePermission(perm);
        } catch (ex) {
          console.error(ex);
        }
      });
  },

  async deleteByHost(aHost) {
    // Clearing by host also clears associated subdomains.
    this._deleteInternal(({ principal }) => {
      let toBeRemoved = false;
      try {
        toBeRemoved = Services.eTLD.hasRootDomain(principal.host, aHost);
      } catch (ex) {}
      return toBeRemoved;
    });
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    // If we don't isolate by private browsing / user context we need to clear
    // the pattern field. Otherwise permissions returned by the permission
    // manager will never match. The permission manager strips these fields when
    // their prefs are set to `false`.
    if (!lazy.permissionManagerIsolateByPrivateBrowsing) {
      delete aOriginAttributesPattern.privateBrowsingId;
    }
    if (!lazy.permissionManagerIsolateByUserContext) {
      delete aOriginAttributesPattern.userContextId;
    }
    this._deleteInternal(({ principal }) =>
      hasSite({ principal }, aSchemelessSite, aOriginAttributesPattern)
    );
  },

  async deleteByRange(aFrom) {
    Services.perms.removeByTypeSince("storageAccessAPI", aFrom / 1000);
  },

  async deleteAll() {
    Services.perms.removeByType("storageAccessAPI");
  },
};

const HistoryCleaner = {
  deleteByHost(aHost) {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return lazy.PlacesUtils.history.removeByFilter({ host: "." + aHost });
  },

  deleteByPrincipal(aPrincipal) {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return lazy.PlacesUtils.history.removeByFilter({ host: aPrincipal.host });
  },

  deleteBySite(aSchemelessSite) {
    return this.deleteByHost(aSchemelessSite);
  },

  deleteByRange(aFrom, aTo) {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return lazy.PlacesUtils.history.removeVisitsByFilter({
      beginDate: new Date(aFrom / 1000),
      endDate: new Date(aTo / 1000),
    });
  },

  deleteAll() {
    if (!AppConstants.MOZ_PLACES) {
      return Promise.resolve();
    }
    return lazy.PlacesUtils.history.clear();
  },
};

const SessionHistoryCleaner = {
  async deleteByHost(aHost) {
    // Session storage and history also clear subdomains of aHost.
    Services.obs.notifyObservers(null, "browser:purge-sessionStorage", aHost);
    Services.obs.notifyObservers(
      null,
      "browser:purge-session-history-for-domain",
      aHost
    );
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteBySite(aSchemelessSite, _aOriginAttributesPattern) {
    // TODO: aOriginAttributesPattern.
    return this.deleteByHost(aSchemelessSite, {});
  },

  async deleteByRange(aFrom) {
    Services.obs.notifyObservers(
      null,
      "browser:purge-session-history",
      String(aFrom)
    );
  },

  async deleteAll() {
    Services.obs.notifyObservers(null, "browser:purge-session-history");
  },
};

const AuthTokensCleaner = {
  // TODO: Bug 1726742
  async deleteByPrincipal(aPrincipal, aIsUserRequest) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  // TODO: Bug 1726742
  async deleteBySite(
    _aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  async deleteAll() {
    let sdr = Cc["@mozilla.org/security/sdr;1"].getService(
      Ci.nsISecretDecoderRing
    );
    sdr.logoutAndTeardown();
  },
};

const AuthCacheCleaner = {
  // TODO: Bug 1726743
  async deleteByPrincipal(aPrincipal, aIsUserRequest) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  // TODO: Bug 1726743
  async deleteBySite(
    _aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.obs.notifyObservers(null, "net:clear-active-logins");
      aResolve();
    });
  },
};

// Type of the shutdown exception permission.
const SHUTDOWN_EXCEPTION_PERMISSION = "cookie";

const ShutdownExceptionsCleaner = {
  async _deleteInternal(filter) {
    Services.perms.all
      .filter(({ type }) => type == SHUTDOWN_EXCEPTION_PERMISSION)
      .filter(filter)
      .forEach(perm => {
        try {
          Services.perms.removePermission(perm);
        } catch (ex) {
          console.error(ex);
        }
      });
  },

  async deleteByHost(aHost) {
    this._deleteInternal(({ principal }) => {
      let { host: principalHost } = principal;
      if (!principalHost?.length) {
        return false;
      }
      return Services.eTLD.hasRootDomain(principal.host, aHost);
    });
  },

  async deleteByPrincipal(aPrincipal) {
    Services.perms.removeFromPrincipal(
      aPrincipal,
      SHUTDOWN_EXCEPTION_PERMISSION
    );
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    // If we don't isolate by private browsing / user context we need to clear
    // the pattern field. Otherwise permissions returned by the permission
    // manager will never match. The permission manager strips these fields when
    // their prefs are set to `false`.
    if (!lazy.permissionManagerIsolateByPrivateBrowsing) {
      delete aOriginAttributesPattern.privateBrowsingId;
    }
    if (!lazy.permissionManagerIsolateByUserContext) {
      delete aOriginAttributesPattern.userContextId;
    }

    this._deleteInternal(({ principal }) =>
      hasSite({ principal }, aSchemelessSite, aOriginAttributesPattern)
    );
  },

  async deleteByRange(aFrom) {
    Services.perms.removeByTypeSince(
      SHUTDOWN_EXCEPTION_PERMISSION,
      aFrom / 1000
    );
  },

  async deleteByOriginAttributes(aOriginAttributesString) {
    Services.perms.removePermissionsWithAttributes(
      aOriginAttributesString,
      [SHUTDOWN_EXCEPTION_PERMISSION],
      []
    );
  },

  async deleteAll() {
    Services.perms.removeByType(SHUTDOWN_EXCEPTION_PERMISSION);
  },
};

const PermissionsCleaner = {
  _deleteInternal(filter) {
    Services.perms.all
      // Skip shutdown exception permission because it is handled by ShutDownExceptionsCleaner
      .filter(({ type }) => type != SHUTDOWN_EXCEPTION_PERMISSION)
      .filter(filter)
      .forEach(perm => {
        try {
          Services.perms.removePermission(perm);
        } catch (ex) {
          console.error(ex);
        }
      });
  },

  _thirdPartyStoragePermissionMatchesHost(permissionType, aHost) {
    if (
      !permissionType.startsWith("3rdPartyStorage^") &&
      !permissionType.startsWith("3rdPartyFrameStorage^")
    ) {
      return false;
    }
    let [, site] = permissionType.split("^");
    let uri;
    try {
      uri = Services.io.newURI(site);
    } catch (ex) {
      return false;
    }
    return Services.eTLD.hasRootDomain(uri.host, aHost);
  },

  _getPrincipalHost(principal) {
    try {
      return principal.host;
    } catch (e) {
      return null;
    }
  },

  async deleteByHost(aHost) {
    this._deleteInternal(({ principal, type }) => {
      let principalHost = this._getPrincipalHost(principal);
      if (!principalHost?.length) {
        return false;
      }
      if (Services.eTLD.hasRootDomain(principalHost, aHost)) {
        return true;
      }

      return this._thirdPartyStoragePermissionMatchesHost(type, aHost);
    });
  },

  async deleteByPrincipal(aPrincipal) {
    this._deleteInternal(({ principal, type }) => {
      if (principal.equals(aPrincipal)) {
        return true;
      }
      let principalHost = this._getPrincipalHost(aPrincipal);
      if (!principalHost?.length) {
        return false;
      }
      return this._thirdPartyStoragePermissionMatchesHost(type, principalHost);
    });
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    // If we don't isolate by private browsing / user context we need to clear
    // the pattern field. Otherwise permissions returned by the permission
    // manager will never match. The permission manager strips these fields when
    // their prefs are set to `false`.
    if (!lazy.permissionManagerIsolateByPrivateBrowsing) {
      delete aOriginAttributesPattern.privateBrowsingId;
    }
    if (!lazy.permissionManagerIsolateByUserContext) {
      delete aOriginAttributesPattern.userContextId;
    }

    this._deleteInternal(
      ({ principal, type }) =>
        hasSite({ principal }, aSchemelessSite, aOriginAttributesPattern) ||
        this._thirdPartyStoragePermissionMatchesHost(type, aSchemelessSite)
    );
  },

  async deleteByRange(aFrom) {
    Services.perms.removeAllSinceWithTypeExceptions(aFrom / 1000, [
      SHUTDOWN_EXCEPTION_PERMISSION,
    ]);
  },

  async deleteByOriginAttributes(aOriginAttributesString) {
    Services.perms.removePermissionsWithAttributes(
      aOriginAttributesString,
      [],
      [SHUTDOWN_EXCEPTION_PERMISSION]
    );
  },

  async deleteAll() {
    Services.perms.removeAllExceptTypes([SHUTDOWN_EXCEPTION_PERMISSION]);
  },
};

const PreferencesCleaner = {
  deleteByHost(aHost, aOriginAttributes = {}) {
    aOriginAttributes =
      ChromeUtils.fillNonDefaultOriginAttributes(aOriginAttributes);

    let loadContext;
    if (
      aOriginAttributes.privateBrowsingId ==
      Services.scriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID
    ) {
      loadContext = Cu.createLoadContext();
    } else {
      loadContext = Cu.createPrivateLoadContext();
    }

    // Also clears subdomains of aHost.
    return new Promise((aResolve, aReject) => {
      let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
        Ci.nsIContentPrefService2
      );
      cps2.removeBySubdomain(aHost, loadContext, {
        handleCompletion: aReason => {
          if (aReason === cps2.COMPLETE_ERROR) {
            aReject();
          } else {
            aResolve();
          }
        },
      });
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    // If aOriginAttributesPattern does not specify private or normal browsing
    // clear both.
    let loadContext = null;

    // If the pattern filters by normal or private browsing mode only clear that mode.
    if (aOriginAttributesPattern.privateBrowsingId != null) {
      // The default private browsing ID is 0 which is non private browsing mode
      // / normal mode.
      let isPrivateBrowsing =
        aOriginAttributesPattern.privateBrowsingId !=
        Ci.nsIScriptSecurityManager.DEFAULT_PRIVATE_BROWSING_ID;
      loadContext = isPrivateBrowsing
        ? Cu.createPrivateLoadContext()
        : Cu.createLoadContext();
    }

    let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    await new Promise((aResolve, aReject) => {
      cps2.removeBySubdomain(aSchemelessSite, loadContext, {
        handleCompletion: aReason => {
          if (aReason === cps2.COMPLETE_ERROR) {
            aReject();
          } else {
            aResolve();
          }
        },
      });
    });
  },

  async deleteByRange(aFrom) {
    let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    await new Promise((aResolve, aReject) => {
      cps2.removeAllDomainsSince(aFrom / 1000, null, {
        handleCompletion: aReason => {
          if (aReason === cps2.COMPLETE_ERROR) {
            aReject();
          } else {
            aResolve();
          }
        },
      });
    });
  },

  async deleteAll() {
    let cps2 = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );

    await new Promise((aResolve, aReject) => {
      cps2.removeAllDomains(null, {
        handleCompletion: aReason => {
          if (aReason === cps2.COMPLETE_ERROR) {
            aReject();
          } else {
            aResolve();
          }
        },
      });
    });
  },
};

const ClientAuthRememberCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let cars = Cc[
      "@mozilla.org/security/clientAuthRememberService;1"
    ].getService(Ci.nsIClientAuthRememberService);

    cars.deleteDecisionsByHost(aHost, aOriginAttributes);
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    let cars = Cc[
      "@mozilla.org/security/clientAuthRememberService;1"
    ].getService(Ci.nsIClientAuthRememberService);

    cars
      .getDecisions()
      .filter(({ asciiHost, entryKey }) => {
        // Get the origin attributes which are in the third component of the
        // entryKey. ',' is used as the delimiter.
        let originSuffixEncoded = entryKey.split(",")[2];
        let originAttributes;

        if (originSuffixEncoded) {
          try {
            // Decoding the suffix or parsing the origin attributes can fail. In
            // this case we won't match the partitionKey, but we can still match
            // the asciiHost.
            let originSuffix = decodeURIComponent(originSuffixEncoded);
            originAttributes =
              ChromeUtils.CreateOriginAttributesFromOriginSuffix(originSuffix);
          } catch (e) {
            console.error(e);
          }
        }

        return hasSite(
          {
            host: asciiHost,
            originAttributes,
          },
          aSchemelessSite,
          aOriginAttributesPattern
        );
      })
      .forEach(({ entryKey }) => cars.forgetRememberedDecision(entryKey));
  },

  async deleteAll() {
    let cars = Cc[
      "@mozilla.org/security/clientAuthRememberService;1"
    ].getService(Ci.nsIClientAuthRememberService);
    cars.clearRememberedDecisions();
  },
};

const HSTSCleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let sss = Cc["@mozilla.org/ssservice;1"].getService(
      Ci.nsISiteSecurityService
    );
    let uri = Services.io.newURI("https://" + aHost);
    sss.resetState(
      uri,
      aOriginAttributes,
      Ci.nsISiteSecurityService.RootDomain
    );
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteBySite(aSchemelessSite, _aOriginAttributesPattern) {
    // TODO: aOriginAttributesPattern.
    let sss = Cc["@mozilla.org/ssservice;1"].getService(
      Ci.nsISiteSecurityService
    );

    // Add brackets to IPv6 sites to ensure URI creation succeeds.
    let uri = Services.io.newURI("https://" + aSchemelessSite);
    sss.resetState(uri, {}, Ci.nsISiteSecurityService.BaseDomain);
  },

  async deleteAll() {
    // Clear site security settings - no support for ranges in this
    // interface either, so we clearAll().
    let sss = Cc["@mozilla.org/ssservice;1"].getService(
      Ci.nsISiteSecurityService
    );
    sss.clearAll();
  },
};

const EMECleaner = {
  async deleteByHost(aHost, aOriginAttributes) {
    let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].getService(
      Ci.mozIGeckoMediaPluginChromeService
    );
    mps.forgetThisSite(aHost, JSON.stringify(aOriginAttributes));
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  async deleteBySite(aSchemelessSite, _aOriginAttributesPattern) {
    // TODO: aOriginAttributesPattern.
    let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].getService(
      Ci.mozIGeckoMediaPluginChromeService
    );
    mps.forgetThisBaseDomain(aSchemelessSite);
  },

  deleteAll() {
    // Not implemented.
    return Promise.resolve();
  },
};

const ReportsCleaner = {
  deleteByHost(aHost) {
    // Also clears subdomains of aHost.
    return new Promise(aResolve => {
      Services.obs.notifyObservers(null, "reporting:purge-host", aHost);
      aResolve();
    });
  },

  deleteByPrincipal(aPrincipal) {
    return this.deleteByHost(aPrincipal.host, aPrincipal.originAttributes);
  },

  deleteBySite(aSchemelessSite, _aOriginAttributesPattern) {
    // TODO: aOriginAttributesPattern.
    return this.deleteByHost(aSchemelessSite, {});
  },

  deleteAll() {
    return new Promise(aResolve => {
      Services.obs.notifyObservers(null, "reporting:purge-all");
      aResolve();
    });
  },
};

const ContentBlockingCleaner = {
  deleteAll() {
    return lazy.TrackingDBService.clearAll();
  },

  async deleteByPrincipal(aPrincipal, aIsUserRequest) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  async deleteBySite(
    _aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  deleteByRange(aFrom) {
    return lazy.TrackingDBService.clearSince(aFrom);
  },
};

/**
 * The about:home startup cache, if it exists, might contain information
 * about where the user has been, or what they've downloaded.
 */
const AboutHomeStartupCacheCleaner = {
  async deleteByPrincipal(aPrincipal, aIsUserRequest) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  async deleteBySite(
    _aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  deleteAll() {
    // This cleaner only makes sense on Firefox desktop, which is the only
    // application that uses the about:home startup cache.
    if (!AppConstants.MOZ_BUILD_APP == "browser") {
      return Promise.resolve();
    }

    return new Promise((aResolve, aReject) => {
      let lci = Services.loadContextInfo.default;
      let storage = Services.cache2.diskCacheStorage(lci);
      let uri = Services.io.newURI("about:home");
      try {
        storage.asyncDoomURI(uri, "", {
          onCacheEntryDoomed(aResult) {
            if (
              Components.isSuccessCode(aResult) ||
              aResult == Cr.NS_ERROR_NOT_AVAILABLE
            ) {
              aResolve();
            } else {
              aReject({
                message: "asyncDoomURI for about:home failed",
              });
            }
          },
        });
      } catch (e) {
        aReject({
          message: "Failed to doom about:home startup cache entry",
        });
      }
    });
  },
};

const PreflightCacheCleaner = {
  // TODO: Bug 1727141: We should call the cache to clear by principal, rather
  // than over-clearing for user requests or bailing out for programmatic calls.
  async deleteByPrincipal(aPrincipal, aIsUserRequest) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  // TODO: Bug 1727141 (see deleteByPrincipal).
  async deleteBySite(
    _aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    if (!aIsUserRequest) {
      return;
    }
    await this.deleteAll();
  },

  async deleteAll() {
    Cc[`@mozilla.org/network/protocol;1?name=http`]
      .getService(Ci.nsIHttpProtocolHandler)
      .clearCORSPreflightCache();
  },
};

const IdentityCredentialStorageCleaner = {
  async deleteAll() {
    if (
      Services.prefs.getBoolPref(
        "dom.security.credentialmanagement.identity.enabled",
        false
      )
    ) {
      lazy.IdentityCredentialStorageService.clear();
    }
  },

  async deleteByPrincipal(aPrincipal) {
    if (
      Services.prefs.getBoolPref(
        "dom.security.credentialmanagement.identity.enabled",
        false
      )
    ) {
      lazy.IdentityCredentialStorageService.deleteFromPrincipal(aPrincipal);
    }
  },

  async deleteBySite(
    aSchemelessSite,
    _aOriginAttributesPattern,
    aIsUserRequest
  ) {
    // TODO: aOriginAttributesPattern.
    if (!aIsUserRequest) {
      return;
    }
    if (
      Services.prefs.getBoolPref(
        "dom.security.credentialmanagement.identity.enabled",
        false
      )
    ) {
      lazy.IdentityCredentialStorageService.deleteFromBaseDomain(
        aSchemelessSite
      );
    }
  },

  async deleteByRange(aFrom, aTo) {
    if (
      Services.prefs.getBoolPref(
        "dom.security.credentialmanagement.identity.enabled",
        false
      )
    ) {
      lazy.IdentityCredentialStorageService.deleteFromTimeRange(aFrom, aTo);
    }
  },

  async deleteByHost(aHost, aOriginAttributes) {
    if (
      Services.prefs.getBoolPref(
        "dom.security.credentialmanagement.identity.enabled",
        false
      )
    ) {
      // Delete data from both HTTP and HTTPS sites.
      let httpURI = Services.io.newURI("http://" + aHost);
      let httpsURI = Services.io.newURI("https://" + aHost);
      let httpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
        httpURI,
        aOriginAttributes
      );
      let httpsPrincipal =
        Services.scriptSecurityManager.createContentPrincipal(
          httpsURI,
          aOriginAttributes
        );
      lazy.IdentityCredentialStorageService.deleteFromPrincipal(httpPrincipal);
      lazy.IdentityCredentialStorageService.deleteFromPrincipal(httpsPrincipal);
    }
  },

  async deleteByOriginAttributes(aOriginAttributesString) {
    if (
      Services.prefs.getBoolPref(
        "dom.security.credentialmanagement.identity.enabled",
        false
      )
    ) {
      lazy.IdentityCredentialStorageService.deleteFromOriginAttributesPattern(
        aOriginAttributesString
      );
    }
  },
};

const BounceTrackingProtectionStateCleaner = {
  async deleteAll() {
    if (
      lazy.bounceTrackingProtectionMode ==
      Ci.nsIBounceTrackingProtection.MODE_DISABLED
    ) {
      return;
    }
    lazy.bounceTrackingProtection.clearAll();
  },

  async deleteByPrincipal(aPrincipal) {
    if (
      lazy.bounceTrackingProtectionMode ==
      Ci.nsIBounceTrackingProtection.MODE_DISABLED
    ) {
      return;
    }
    let { baseDomain, originAttributes } = aPrincipal;
    lazy.bounceTrackingProtection.clearBySiteHostAndOriginAttributes(
      baseDomain,
      originAttributes
    );
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    if (
      lazy.bounceTrackingProtectionMode ==
      Ci.nsIBounceTrackingProtection.MODE_DISABLED
    ) {
      return;
    }
    lazy.bounceTrackingProtection.clearBySiteHostAndOriginAttributesPattern(
      aSchemelessSite,
      aOriginAttributesPattern
    );
  },

  async deleteByRange(aFrom, aTo) {
    if (
      lazy.bounceTrackingProtectionMode ==
      Ci.nsIBounceTrackingProtection.MODE_DISABLED
    ) {
      return;
    }
    lazy.bounceTrackingProtection.clearByTimeRange(aFrom, aTo);
  },

  async deleteByHost(aHost, aOriginAttributesPattern = {}) {
    if (
      lazy.bounceTrackingProtectionMode ==
      Ci.nsIBounceTrackingProtection.MODE_DISABLED
    ) {
      return;
    }
    let baseDomain = Services.eTLD.getSchemelessSiteFromHost(aHost);
    lazy.bounceTrackingProtection.clearBySiteHostAndOriginAttributesPattern(
      baseDomain,
      aOriginAttributesPattern
    );
  },

  async deleteByOriginAttributes(aOriginAttributesPatternString) {
    if (
      lazy.bounceTrackingProtectionMode ==
      Ci.nsIBounceTrackingProtection.MODE_DISABLED
    ) {
      return;
    }
    lazy.bounceTrackingProtection.clearByOriginAttributesPattern(
      aOriginAttributesPatternString
    );
  },
};

const StoragePermissionsCleaner = {
  async deleteByRange(aFrom) {
    // We lack the ability to clear by range, but can clear from a certain time to now
    // Convert aFrom from microseconds to ms
    Services.perms.removeByTypeSince("storage-access", aFrom / 1000);

    let persistentStoragePermissions = Services.perms.getAllByTypeSince(
      "persistent-storage",
      aFrom / 1000
    );
    persistentStoragePermissions.forEach(perm => {
      // If it is an Addon Principal, do nothing.
      // We want their persistant-storage permissions to remain (Bug 1907732)
      if (this._isAddonPrincipal(perm.principal)) {
        return;
      }
      Services.perms.removePermission(perm);
    });
  },

  async deleteByPrincipal(aPrincipal) {
    Services.perms.removeFromPrincipal(aPrincipal, "storage-access");

    // Only remove persistent-storage if it is not an extension principal (Bug 1907732)
    if (!this._isAddonPrincipal(aPrincipal)) {
      Services.perms.removeFromPrincipal(aPrincipal, "persistent-storage");
    }
  },

  async deleteByHost(aHost) {
    let permissions = this._getStoragePermissions();
    for (let perm of permissions) {
      if (Services.eTLD.hasRootDomain(perm.principal.host, aHost)) {
        Services.perms.removePermission(perm);
      }
    }
  },

  async deleteBySite(aSchemelessSite, aOriginAttributesPattern) {
    // If we don't isolate by private browsing / user context we need to clear
    // the pattern field. Otherwise permissions returned by the permission
    // manager will never match. The permission manager strips these fields when
    // their prefs are set to `false`.
    if (!lazy.permissionManagerIsolateByPrivateBrowsing) {
      delete aOriginAttributesPattern.privateBrowsingId;
    }
    if (!lazy.permissionManagerIsolateByUserContext) {
      delete aOriginAttributesPattern.userContextId;
    }

    let permissions = this._getStoragePermissions();
    for (let perm of permissions) {
      let { principal } = perm;
      if (hasSite({ principal }, aSchemelessSite, aOriginAttributesPattern)) {
        Services.perms.removePermission(perm);
      }
    }
  },

  async deleteByLocalFiles() {
    let permissions = this._getStoragePermissions();
    for (let perm of permissions) {
      if (perm.principal.schemeIs("file")) {
        Services.perms.removePermission(perm);
      }
    }
  },

  async deleteAll() {
    Services.perms.removeByType("storage-access");

    // We don't want to clear the persistent-storage permission from addons (Bug 1907732)
    let persistentStoragePermissions = Services.perms.getAllByTypes([
      "persistent-storage",
    ]);
    persistentStoragePermissions.forEach(perm => {
      if (this._isAddonPrincipal(perm.principal)) {
        return;
      }

      Services.perms.removePermission(perm);
    });
  },

  _getStoragePermissions() {
    let storagePermissions = Services.perms.getAllByTypes([
      "storage-access",
      "persistent-storage",
    ]);

    return storagePermissions.filter(
      permission =>
        !this._isAddonPrincipal(permission.principal) ||
        permission.type == "storage-access"
    );
  },

  _isAddonPrincipal(aPrincipal) {
    return (
      // AddonPolicy() returns a WebExtensionPolicy that has been registered before,
      // typically during extension startup. Since Disabled or uninstalled add-ons
      // don't appear there, we should use schemeIs instead
      aPrincipal.schemeIs("moz-extension")
    );
  },
};

// Here the map of Flags-Cleaners.
const FLAGS_MAP = [
  {
    flag: Ci.nsIClearDataService.CLEAR_CERT_EXCEPTIONS,
    cleaners: [CertCleaner],
  },

  { flag: Ci.nsIClearDataService.CLEAR_COOKIES, cleaners: [CookieCleaner] },

  {
    flag: Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
    cleaners: [NetworkCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_IMAGE_CACHE,
    cleaners: [ImageCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CSS_CACHE,
    cleaners: [CSSCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_MESSAGING_LAYER_SECURITY_STATE,
    cleaners: [MessagingLayerSecurityStateCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_JS_CACHE,
    cleaners: [JSCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CLIENT_AUTH_REMEMBER_SERVICE,
    cleaners: [ClientAuthRememberCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_DOWNLOADS,
    cleaners: [DownloadsCleaner, AboutHomeStartupCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES,
    cleaners: [MediaDevicesCleaner],
  },

  { flag: Ci.nsIClearDataService.CLEAR_DOM_QUOTA, cleaners: [QuotaCleaner] },

  {
    flag: Ci.nsIClearDataService.CLEAR_PREDICTOR_NETWORK_DATA,
    cleaners: [PredictorNetworkCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_DOM_PUSH_NOTIFICATIONS,
    cleaners: [PushNotificationsCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_HISTORY,
    cleaners: [
      HistoryCleaner,
      SessionHistoryCleaner,
      AboutHomeStartupCacheCleaner,
    ],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_AUTH_TOKENS,
    cleaners: [AuthTokensCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
    cleaners: [AuthCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_SITE_PERMISSIONS,
    cleaners: [PermissionsCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CONTENT_PREFERENCES,
    cleaners: [PreferencesCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_HSTS,
    cleaners: [HSTSCleaner],
  },

  { flag: Ci.nsIClearDataService.CLEAR_EME, cleaners: [EMECleaner] },

  { flag: Ci.nsIClearDataService.CLEAR_REPORTS, cleaners: [ReportsCleaner] },

  {
    flag: Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS,
    cleaners: [StorageAccessCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CONTENT_BLOCKING_RECORDS,
    cleaners: [ContentBlockingCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_PREFLIGHT_CACHE,
    cleaners: [PreflightCacheCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_CREDENTIAL_MANAGER_STATE,
    cleaners: [IdentityCredentialStorageCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXCEPTION,
    cleaners: [CookieBannerExceptionCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_COOKIE_BANNER_EXECUTED_RECORD,
    cleaners: [CookieBannerExecutedRecordCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_FINGERPRINTING_PROTECTION_STATE,
    cleaners: [FingerprintingProtectionStateCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_BOUNCE_TRACKING_PROTECTION_STATE,
    cleaners: [BounceTrackingProtectionStateCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_STORAGE_PERMISSIONS,
    cleaners: [StoragePermissionsCleaner],
  },

  {
    flag: Ci.nsIClearDataService.CLEAR_SHUTDOWN_EXCEPTIONS,
    cleaners: [ShutdownExceptionsCleaner],
  },
];

export function ClearDataService() {
  this._initialize();
}

ClearDataService.prototype = Object.freeze({
  classID: Components.ID("{0c06583d-7dd8-4293-b1a5-912205f779aa}"),
  QueryInterface: ChromeUtils.generateQI(["nsIClearDataService"]),

  _initialize() {
    // Let's start all the service we need to cleanup data.

    // This is mainly needed for GeckoView that doesn't start QMS on startup
    // time.
    if (!Services.qms) {
      console.error("Failed initializiation of QuotaManagerService.");
    }
  },

  deleteDataFromLocalFiles(aIsUserRequest, aFlags, aCallback) {
    if (!aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      // Some of the 'Cleaners' do not support clearing data for
      // local files. Ignore those.
      if (aCleaner.deleteByLocalFiles) {
        // A generic originAttributes dictionary.
        return aCleaner.deleteByLocalFiles({});
      }
      return Promise.resolve();
    });
  },

  deleteDataFromHost(aHost, aIsUserRequest, aFlags, aCallback) {
    if (!aHost || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      // Some of the 'Cleaners' do not support to delete by principal. Let's
      // use deleteAll() as fallback.
      if (aCleaner.deleteByHost) {
        // A generic originAttributes dictionary.
        return aCleaner.deleteByHost(aHost, {});
      }
      // The user wants to delete data. Let's remove as much as we can.
      if (aIsUserRequest) {
        return aCleaner.deleteAll();
      }
      // We don't want to delete more than what is strictly required.
      return Promise.resolve();
    });
  },

  deleteDataFromSite(
    aSchemelessSite,
    aOriginAttributesPattern,
    aIsUserRequest,
    aFlags,
    aCallback
  ) {
    if (!aSchemelessSite?.length || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    // For debug builds validate aSchemelessSite.
    if (AppConstants.DEBUG) {
      let schemelessSiteComputed =
        Services.eTLD.getSchemelessSiteFromHost(aSchemelessSite);
      if (schemelessSiteComputed != aSchemelessSite) {
        throw new Error(
          `deleteDataFromSite called with invalid aSchemelessSite '${aSchemelessSite}'. Expected site is '${schemelessSiteComputed}'`
        );
      }
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner =>
      aCleaner.deleteBySite(
        aSchemelessSite,
        aOriginAttributesPattern,
        aIsUserRequest
      )
    );
  },

  deleteDataFromSiteAndOriginAttributesPatternString(
    aSchemelessSite,
    aOriginAttributesPatternString,
    aIsUserRequest,
    aFlags,
    aCallback
  ) {
    if (!aSchemelessSite || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    // Parse the pattern string.
    let originAttributesPattern = {};
    if (aOriginAttributesPatternString?.length) {
      originAttributesPattern = JSON.parse(aOriginAttributesPatternString);
    }

    // Call the other variant which expects a OriginAttributesPattern object.
    return this.deleteDataFromSite(
      aSchemelessSite,
      originAttributesPattern,
      aIsUserRequest,
      aFlags,
      aCallback
    );
  },

  deleteDataFromPrincipal(aPrincipal, aIsUserRequest, aFlags, aCallback) {
    if (!aPrincipal || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner =>
      aCleaner.deleteByPrincipal(aPrincipal, aIsUserRequest)
    );
  },

  deleteDataInTimeRange(aFrom, aTo, aIsUserRequest, aFlags, aCallback) {
    if (aFrom > aTo || !aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      // Some of the 'Cleaners' do not support to delete by range. Let's use
      // deleteAll() as fallback.
      if (aCleaner.deleteByRange) {
        return aCleaner.deleteByRange(aFrom, aTo);
      }
      // The user wants to delete data. Let's remove as much as we can.
      if (aIsUserRequest) {
        return aCleaner.deleteAll();
      }
      // We don't want to delete more than what is strictly required.
      return Promise.resolve();
    });
  },

  deleteData(aFlags, aCallback) {
    if (!aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    return this._deleteInternal(aFlags, aCallback, aCleaner => {
      return aCleaner.deleteAll();
    });
  },

  deleteDataFromOriginAttributesPattern(aPattern, aCallback) {
    if (!aPattern) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    let patternString = JSON.stringify(aPattern);
    // XXXtt remove clear-origin-attributes-data entirely
    Services.obs.notifyObservers(
      null,
      "clear-origin-attributes-data",
      patternString
    );

    if (!aCallback) {
      aCallback = {
        onDataDeleted: () => {},
      };
    }
    return this._deleteInternal(
      Ci.nsIClearDataService.CLEAR_ALL,
      aCallback,
      aCleaner => {
        if (aCleaner.deleteByOriginAttributes) {
          return aCleaner.deleteByOriginAttributes(patternString);
        }

        // We don't want to delete more than what is strictly required.
        return Promise.resolve();
      }
    );
  },

  deleteUserInteractionForClearingHistory(
    aPrincipalsWithStorage,
    aFrom,
    aCallback
  ) {
    if (!aCallback) {
      return Cr.NS_ERROR_INVALID_ARG;
    }

    StorageAccessCleaner.deleteExceptPrincipals(aPrincipalsWithStorage, aFrom)
      .then(() => {
        aCallback.onDataDeleted(0);
      })
      .catch(() => {
        // This is part of clearing storageAccessAPI permissions, thus return
        // an appropriate error flag.
        aCallback.onDataDeleted(Ci.nsIClearDataService.CLEAR_PERMISSIONS);
      });
    return Cr.NS_OK;
  },

  cleanupAfterDeletionAtShutdown(aFlags, aCallback) {
    return this._deleteInternal(aFlags, aCallback, async aCleaner => {
      if (aCleaner.cleanupAfterDeletionAtShutdown) {
        await aCleaner.cleanupAfterDeletionAtShutdown();
      }
    });
  },

  hostMatchesSite(
    aHost,
    aOriginAttributes,
    aSchemelessSite,
    aOriginAttributesPattern = {}
  ) {
    return hasSite(
      { host: aHost, originAttributes: aOriginAttributes },
      aSchemelessSite,
      aOriginAttributesPattern
    );
  },

  // This internal method uses aFlags against FLAGS_MAP in order to retrieve a
  // list of 'Cleaners'. For each of them, the aHelper callback retrieves a
  // promise object. All these promise objects are resolved before calling
  // onDataDeleted.
  _deleteInternal(aFlags, aCallback, aHelper) {
    let resultFlags = 0;
    let promises = FLAGS_MAP.filter(c => aFlags & c.flag).map(c => {
      return Promise.all(
        c.cleaners.map(cleaner => {
          return aHelper(cleaner).catch(e => {
            console.error(e);
            resultFlags |= c.flag;
          });
        })
      );
      // Let's collect the failure in resultFlags.
    });
    Promise.all(promises).then(() => {
      aCallback.onDataDeleted(resultFlags);
    });
    return Cr.NS_OK;
  },
});

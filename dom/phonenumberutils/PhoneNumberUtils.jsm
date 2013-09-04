/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

"use strict";

this.EXPORTED_SYMBOLS = ["PhoneNumberUtils"];

const DEBUG = false;
function debug(s) { if(DEBUG) dump("-*- PhoneNumberutils: " + s + "\n"); }

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import("resource://gre/modules/PhoneNumber.jsm");
Cu.import("resource://gre/modules/mcc_iso3166_table.jsm");

#ifdef MOZ_B2G_RIL
XPCOMUtils.defineLazyServiceGetter(this, "mobileConnection",
                                   "@mozilla.org/ril/content-helper;1",
                                   "nsIRILContentHelper");
#endif

this.PhoneNumberUtils = {
  //  1. See whether we have a network mcc
  //  2. If we don't have that, look for the simcard mcc
  //  3. If we don't have that or its 0 (not activated), pick up the last used mcc
  //  4. If we don't have, default to some mcc

  // mcc for Brasil
  _mcc: '724',

  getCountryName: function getCountryName() {
    let mcc;
    let countryName;

#ifdef MOZ_B2G_RIL
    // Get network mcc
    let voice = mobileConnection.voiceConnectionInfo;
    if (voice && voice.network && voice.network.mcc) {
      mcc = voice.network.mcc;
    }

    // Get SIM mcc
    let iccInfo = mobileConnection.iccInfo;
    if (!mcc && iccInfo.mcc) {
      mcc = iccInfo.mcc;
    }

    // Attempt to grab last known sim mcc from prefs
    if (!mcc) {
      try {
        mcc = Services.prefs.getCharPref("ril.lastKnownSimMcc");
      } catch (e) {}
    }

    // Set to default mcc
    if (!mcc) {
      mcc = this._mcc;
    }
#else

    // Attempt to grab last known sim mcc from prefs
    if (!mcc) {
      try {
        mcc = Services.prefs.getCharPref("ril.lastKnownSimMcc");
      } catch (e) {}
    }

    if (!mcc) {
      mcc = this._mcc;
    }
#endif

    countryName = MCC_ISO3166_TABLE[mcc];
    if (DEBUG) debug("MCC: " + mcc + "countryName: " + countryName);
    return countryName;
  },

  parse: function(aNumber) {
    if (DEBUG) debug("call parse: " + aNumber);
    let result = PhoneNumber.Parse(aNumber, this.getCountryName());

    if (result) {
      let countryName = result.countryName || this.getCountryName();
      let number = null;
      if (countryName) {
        if (Services.prefs.getPrefType("dom.phonenumber.substringmatching." + countryName) == Ci.nsIPrefBranch.PREF_INT) {
          let val = Services.prefs.getIntPref("dom.phonenumber.substringmatching." + countryName);
          if (val) {
            number = result.internationalNumber || result.nationalNumber;
            if (number && number.length > val) {
              number = number.slice(-val);
            }
          }
        }
      }
      Object.defineProperty(result, "nationalMatchingFormat", { value: number, enumerable: true });
      if (DEBUG) {
        debug("InternationalFormat: " + result.internationalFormat);
        debug("InternationalNumber: " + result.internationalNumber);
        debug("NationalNumber: " + result.nationalNumber);
        debug("NationalFormat: " + result.nationalFormat);
        debug("CountryName: " + result.countryName);
        debug("NationalMatchingFormat: " + result.nationalMatchingFormat);
      }
    } else if (DEBUG) {
      debug("NO PARSING RESULT!");
    }
    return result;
  },

  parseWithMCC: function(aNumber, aMCC) {
    let countryName = MCC_ISO3166_TABLE[aMCC];
    if (DEBUG) debug("found country name: " + countryName);
    return PhoneNumber.Parse(aNumber, countryName);
  },

  isPlainPhoneNumber: function isPlainPhoneNumber(aNumber) {
    var isPlain = PhoneNumber.IsPlain(aNumber);
    if (DEBUG) debug("isPlain(" + aNumber + ") " + isPlain);
    return isPlain;
  },

  normalize: function Normalize(aNumber, aNumbersOnly) {
    var normalized = PhoneNumber.Normalize(aNumber, aNumbersOnly);
    if (DEBUG) debug("normalize(" + aNumber + "): " + normalized + ", " + aNumbersOnly);
    return normalized;
  }
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let WSP = {};
Cu.import("resource://gre/modules/WspPduHelper.jsm", WSP);
let WBXML = {};
Cu.import("resource://gre/modules/WbxmlPduHelper.jsm", WBXML);

// set to true to see debug messages
let DEBUG = WBXML.DEBUG_ALL | false;

/**
 * Public identifier for SI
 *
 * @see http://technical.openmobilealliance.org/tech/omna/omna-wbxml-public-docid.aspx
 */
const PUBLIC_IDENTIFIER_SI    = "-//WAPFORUM//DTD SI 1.0//EN";

/**
 * Parse inline date encoded in OPAQUE format.
 *
 * @param data
 *        A wrapped object containing raw PDU data.
 * @param msg
 *        Target object for decoding.
 *
 * @see WAP-167-SERVICEIND-20010731-A, clause 8.2.2
 *
 */
this.OpaqueAsDate = {
  decode: function decode_opaque_as_date(data) {
    let size = WSP.UintVar.decode(data);
    let dateBuf = [0, 0, 0, 0, 0, 0, 0];

    // Maximum length for date is 7 bytes.
    if (size > dateBuf.length)
      size = dateBuf.length

    // Read date date, non-specified parts remain 0.
    for (let i = 0; i < size; i++) {
      dateBuf[i] = WSP.Octet.decode(data);
    }

    // Decode and return result in "YYYY-MM-DDThh:mm:ssZ" form
    let year = ((dateBuf[0] >> 4) & 0x0F) * 1000 + (dateBuf[0] & 0x0F) * 100 +
               ((dateBuf[1] >> 4) & 0x0F) * 10 + (dateBuf[1] & 0x0F);
    let month = ((dateBuf[2] >> 4) & 0x0F) * 10 + (dateBuf[2] & 0x0F);
    let date = ((dateBuf[3] >> 4) & 0x0F) * 10 + (dateBuf[3] & 0x0F);
    let hour = ((dateBuf[4] >> 4) & 0x0F) * 10 + (dateBuf[4] & 0x0F);
    let minute = ((dateBuf[5] >> 4) & 0x0F) * 10 + (dateBuf[5] & 0x0F);
    let second = ((dateBuf[6] >> 4) & 0x0F) * 10 + (dateBuf[6] & 0x0F);
    let dateValue = new Date(Date.UTC(year, month - 1, date, hour, minute, second));

    return dateValue.toISOString().replace(".000", "");
  },
};

this.PduHelper = {

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param contentType
   *        Content type of incoming SI message, should be "text/vnd.wap.si" or
   *        "application/vnd.wap.sic".
   *
   * @return A message object containing attribute content and contentType.
   *         |content| will contain string of decoded SI message if successfully
   *         decoded, or raw data if failed.
   *         |contentType| will be string representing corresponding type of
   *         content.
   */
  parse: function parse_si(data, contentType) {
    // We only need content and contentType
    let msg = {
      contentType: contentType
    };

    /**
     * Message is compressed by WBXML, decode into string.
     *
     * @see WAP-192-WBXML-20010725-A
     */
    if (contentType === "application/vnd.wap.sic") {
      let globalTokenOverride = {};
      globalTokenOverride[0xC3] = {
        number: 0xC3,
        coder: OpaqueAsDate
      };

      let appToken = {
        publicId: PUBLIC_IDENTIFIER_SI,
        tagTokenList: SI_TAG_FIELDS,
        attrTokenList: SI_ATTRIBUTE_FIELDS,
        valueTokenList: SI_VALUE_FIELDS,
        globalTokenOverride: globalTokenOverride
      }

      try {
        let parseResult = WBXML.PduHelper.parse(data, appToken);
        msg.content = parseResult.content;
        msg.contentType = "text/vnd.wap.si";
      } catch (e) {
        // Provide raw data if we failed to parse.
        msg.content = data.array;
      }
      return msg;
    }

    /**
     * Message is plain text, transform raw to string.
     */
    try {
      let stringData = WSP.Octet.decodeMultiple(data, data.array.length);
      msg.content = WSP.PduHelper.decodeStringContent(stringData, "UTF-8");
    } catch (e) {
      // Provide raw data if we failed to parse.
      msg.content = data.array;
    }
    return msg;

  }
};

/**
 * Tag tokens
 *
 * @see WAP-167-SERVICEIND-20010731-A, clause 8.3.1
 */
const SI_TAG_FIELDS = (function () {
  let names = {};
  function add(name, codepage, number) {
    let entry = {
      name: name,
      number: number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  add("si",           0,  0x05);
  add("indication",   0,  0x06);
  add("info",         0,  0x07);
  add("item",         0,  0x08);

  return names;
})();

/**
 * Attribute Tokens
 *
 * @see WAP-167-SERVICEIND-20010731-A, clause 8.3.2
 */
const SI_ATTRIBUTE_FIELDS = (function () {
  let names = {};
  function add(name, value, codepage, number) {
    let entry = {
      name: name,
      value: value,
      number: number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  add("action",       "signal-none",    0,  0x05);
  add("action",       "signal-low",     0,  0x06);
  add("action",       "signal-medium",  0,  0x07);
  add("action",       "signal-high",    0,  0x08);
  add("action",       "delete",         0,  0x09);
  add("created",      "",               0,  0x0A);
  add("href",         "",               0,  0x0B);
  add("href",         "http://",        0,  0x0C);
  add("href",         "http://www.",    0,  0x0D);
  add("href",         "https://",       0,  0x0E);
  add("href",         "https://www.",   0,  0x0F);
  add("si-expires",   "",               0,  0x10);
  add("si-id",        "",               0,  0x11);
  add("class",        "",               0,  0x12);

  return names;
})();

const SI_VALUE_FIELDS = (function () {
  let names = {};
  function add(value, codepage, number) {
    let entry = {
      value: value,
      number: number,
    };
    if (!names[codepage]) {
      names[codepage] = {};
    }
    names[codepage][number] = entry;
  }

  add(".com/",    0,    0x85);
  add(".edu/",    0,    0x86);
  add(".net/",    0,    0x87);
  add(".org/",    0,    0x88);

  return names;
})();

this.EXPORTED_SYMBOLS = [
  // Parser
  "PduHelper",
];

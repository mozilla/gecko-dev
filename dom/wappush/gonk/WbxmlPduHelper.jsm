/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

let WSP = {};
Cu.import("resource://gre/modules/WspPduHelper.jsm", WSP);

/**
 * Token flags
 *
 * @see WAP-192-WBXML-20010725-A, clause 5.8.2
 */
const TAG_TOKEN_ATTR_MASK     = 0x80;
const TAG_TOKEN_CONTENT_MASK  = 0x40;
const TAG_TOKEN_VALUE_MASK    = 0x3F;

/**
 * Global tokens
 *
 * @see WAP-192-WBXML-20010725-A, clause 7.1
 */
const CODE_PAGE_SWITCH_TOKEN  = 0x00;
const TAG_END_TOKEN           = 0x01;
const INLINE_STRING_TOKEN     = 0x03;
const STRING_TABLE_TOKEN      = 0x83;
const OPAQUE_TOKEN            = 0xC3;

// Set to true to enable debug message on all WBXML decoders.
this.DEBUG_ALL = false;

// Enable debug message for WBXML decoder core.
this.DEBUG = DEBUG_ALL | false;

/**
 * Handle WBXML code page switch.
 *
 * @param data
 *        A wrapped object containing raw PDU data.
 * @param decodeInfo
 *        Internal information for decode process.
 *
 * @see WAP-192-WBXML-20010725-A, clause 5.8.4.7.2 and 5.8.1
 */
this.WbxmlCodePageSwitch = {
  decode: function decode_wbxml_code_page_switch(data, decodeInfo) {
    let codePage = WSP.Octet.decode(data);

    if (decodeInfo.currentState === "tag") {
      decodeInfo.currentTokenList.tag = decodeInfo.tokenList.tag[codePage];

      if (!decodeInfo.currentTokenList.tag) {
        throw new Error("Invalid tag code page: " + codePage + ".");
      }

      return "";
    }

    if (decodeInfo.currentState === "attr") {
      decodeInfo.currentTokenList.attr = decodeInfo.tokenList.attr[codePage];
      decodeInfo.currentTokenList.value = decodeInfo.tokenList.value[codePage];

      if (!decodeInfo.currentTokenList.attr ||
          !decodeInfo.currentTokenList.value) {
        throw new Error("Invalid attr code page: " + codePage + ".");
      }

      return "";
    }

    throw new Error("Invalid decoder state: " + decodeInfo.currentState + ".");
  },
};

/**
 * Parse end WBXML encoded message.
 *
 * @param data
 *        A wrapped object containing raw PDU data.
 * @param decodeInfo
 *        Internal information for decode process.
 *
 * @see WAP-192-WBXML-20010725-A, clause 5.8.4.7.1
 *
 */
this.WbxmlEnd = {
  decode: function decode_wbxml_end(data, decodeInfo) {
    let tagInfo = decodeInfo.tagStack.pop();
    return "</" + tagInfo.name + ">";
  },
};

/**
 * Escape XML reserved characters &, <, >, " and ' which may appear in the
 * WBXML-encoded strings in their original form.
 *
 * @param str
 *        A string with potentially unescaped characters
 *
 * @return A string with the &, <, >, " and ' characters turned into XML
 *         character entitites
 *
 * @see WAP-192-WBXML-20010725-A, clause 6.1
 */
this.escapeReservedCharacters = function escape_reserved_characters(str) {
  let dst = "";

  for (var i = 0; i < str.length; i++) {
    switch (str[i]) {
      case '&' : dst += "&amp;" ; break;
      case '<' : dst += "&lt;"  ; break;
      case '>' : dst += "&gt;"  ; break;
      case '"' : dst += "&quot;"; break;
      case '\'': dst += "&apos;"; break;
      default  : dst += str[i];
    }
  }

  return dst;
}

/**
 * Handle string table in WBXML message.
 *
 * @see WAP-192-WBXML-20010725-A, clause 5.7
 */
this.readStringTable = function decode_wbxml_read_string_table(start, stringTable, charset) {
  let end = start;

  // Find end of string
  let stringTableLength = stringTable.length;
  while (end < stringTableLength) {
    if (stringTable[end] === 0) {
      break;
    }
    end++;
  }

  // Read string table
  return WSP.PduHelper.decodeStringContent(stringTable.subarray(start, end),
                                           charset);
};

this.WbxmlStringTable = {
  decode: function decode_wbxml_string_table(data, decodeInfo) {
    let start = WSP.Octet.decode(data);
    let str = readStringTable(start, decodeInfo.stringTable, decodeInfo.charset);

    return escapeReservedCharacters(str);
  }
};

/**
 * Parse inline string in WBXML encoded message.
 *
 * @param data
 *        A wrapped object containing raw PDU data.
 * @param decodeInfo
 *        Internal information for decode process.
 *
 * @see WAP-192-WBXML-20010725-A, clause 5.8.4.1
 *
 */
this.WbxmlInlineString = {
  decode: function decode_wbxml_inline_string(data, decodeInfo) {
    let charCode = WSP.Octet.decode(data);
    let stringData = [];
    while (charCode) {
      stringData.push(charCode);
      charCode = WSP.Octet.decode(data);
    }

    let str = WSP.PduHelper.decodeStringContent(stringData, decodeInfo.charset);

    return escapeReservedCharacters(str);
  },
};

/**
 * Parse inline Opaque data in WBXML encoded message.
 *
 * @param data
 *        A wrapped object containing raw PDU data.
 * @param decodeInfo
 *        Internal information for decode process.
 *
 * @see WAP-192-WBXML-20010725-A, clause 5.8.4.6
 *
 */
this.WbxmlOpaque = {
  decode: function decode_wbxml_inline_opaque(data) {
    // Inline OPAQUE must be decoded based on application definition,
    // so it's illegal to run into OPAQUE type in general decoder.
    throw new Error("OPQAUE decoder is not defined.");
  },
};

this.PduHelper = {

  /**
   * Parse WBXML encoded message into plain text.
   *
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param decodeInfo
   *        Information for decoding, now requires charset and string table.
   * @param appToken
   *        Application-specific token difinition.
   *
   * @return Decoded WBXML message string.
   */
  parseWbxml: function parseWbxml_wbxml(data, decodeInfo, appToken) {

    // Parse token definition to my structure.
    decodeInfo.tokenList = {
      tag: appToken.tagTokenList,
      attr: appToken.attrTokenList,
      value: appToken.valueTokenList
    };
    decodeInfo.tagStack = [];   // tag decode stack
    decodeInfo.currentTokenList = {
      tag: decodeInfo.tokenList.tag[0],
      attr: decodeInfo.tokenList.attr[0],
      value: decodeInfo.tokenList.value[0]
    };
    decodeInfo.currentState = "tag";  // Current decoding state, "tag" or "attr"
                                      // Used to read corresponding code page
                                      // initial state : "tag"

    // Merge global tag tokens into single list, so we don't have
    // to search two lists every time.
    let globalTagTokenList = Object.create(WBXML_GLOBAL_TOKENS);
    if (appToken.globalTokenOverride) {
      let globalTokenOverrideList = appToken.globalTokenOverride;
      for (let token in globalTokenOverrideList) {
        globalTagTokenList[token] = globalTokenOverrideList[token];
      }
    }

    let content = "";
    while (data.offset < data.array.length) {
      // Decode content, might be a new tag token, an end of tag token, or an
      // inline string.

      // Switch to tag domain
      decodeInfo.currentState = "tag";

      let tagToken = WSP.Octet.decode(data);
      let tagTokenValue = tagToken & TAG_TOKEN_VALUE_MASK;

      // Try global tag first, tagToken of string table is 0x83, and will be 0x03
      // in tagTokenValue, which is collision with inline string.
      // So tagToken need to be searched before tagTokenValue.
      let tagInfo = globalTagTokenList[tagToken] ||
                    globalTagTokenList[tagTokenValue];
      if (tagInfo) {
        content += tagInfo.coder.decode(data, decodeInfo);
        continue;
      }

      // Check if application tag token is valid
      tagInfo = decodeInfo.currentTokenList.tag[tagTokenValue];
      if (!tagInfo) {
        throw new Error("Unsupported WBXML token: " + tagTokenValue + ".");
      }

      content += "<" + tagInfo.name;

      if (tagToken & TAG_TOKEN_ATTR_MASK) {
        // Decode attributes, might be a new attribute token, a value token,
        // or an inline string

        // Switch to attr/value domain
        decodeInfo.currentState = "attr";

        let attrSeperator = "";
        while (data.offset < data.array.length) {
          let attrToken = WSP.Octet.decode(data);
          if (attrToken === TAG_END_TOKEN) {
            break;
          }

          let attrInfo = globalTagTokenList[attrToken];
          if (attrInfo) {
            content += attrInfo.coder.decode(data, decodeInfo);
            continue;
          }

          // Check if attribute token is valid
          attrInfo = decodeInfo.currentTokenList.attr[attrToken];
          if (attrInfo) {
            content += attrSeperator + " " + attrInfo.name + "=\"" + attrInfo.value;
            attrSeperator = "\"";
            continue;
          }

          attrInfo = decodeInfo.currentTokenList.value[attrToken];
          if (attrInfo) {
            content += attrInfo.value;
            continue;
          }

          throw new Error("Unsupported WBXML token: " + attrToken + ".");
        }

        content += attrSeperator;
      }

      if (tagToken & TAG_TOKEN_CONTENT_MASK) {
        content += ">";
        decodeInfo.tagStack.push(tagInfo);
        continue;
      }

      content += "/>";
    }

    return content;
  },

  /**
   * @param data
   *        A wrapped object containing raw PDU data.
   * @param appToken
   *        contains application-specific token info, including
   *        {
   *          publicId              : Public identifier of application.
   *          tagToken              : Ojbect defines application tag tokens.
   *                                  In form of
   *                                  Object[TAG_NAME] = Object[TOKEN_NUMBER] =
   *                                  {
   *                                    name: "TOKEN_NAME",
   *                                    number: TOKEN_NUMBER
   *                                  }
   *          attrToken             : Object defines application attribute tokens.
   *                                  Object[ATTR_NAME] = Object[TOKEN_NUMBER] =
   *                                  {
   *                                    name: "ATTR_NAME",
   *                                    value: "ATTR_VALUE",
   *                                    number: TOKEN_NUMBER
   *                                  }
   *                                  For attribute value tokens, assign name as ""
   *          globalTokenOverride   : Object overrides decoding behavior of global tokens.
   *                                  In form of
   *                                  Object[GLOBAL_TOKEN_NUMBER] =
   *                                  {
   *                                    decode: function(data),
   *                                    encode: function(data)
   *                                  }
   *                                  decode() returns decoded text, encode() returns
   *                                  encoded raw data.
   *        }
   *
   * @return A WBXML message object or null in case of errors found.
   */
  parse: function parse_wbxml(data, appToken) {
    let msg = {};

    /**
     * Read WBXML header.
     *
     * @see WAP-192-WBXML-20010725-A, Clause 5.3
     */
    let headerRaw = {};
    headerRaw.version = WSP.Octet.decode(data);
    headerRaw.publicId = WSP.UintVar.decode(data);
    if (headerRaw.publicId === 0) {
      headerRaw.publicIdStr = WSP.UintVar.decode(data);
    }
    headerRaw.charset = WSP.UintVar.decode(data);

    let stringTableLen = WSP.UintVar.decode(data);
    msg.stringTable =
        WSP.Octet.decodeMultiple(data, data.offset + stringTableLen);

    // Transform raw header into user-friendly form
    let entry = WSP.WSP_WELL_KNOWN_CHARSETS[headerRaw.charset];
    if (!entry) {
      throw new Error("Charset is not supported.");
    }
    msg.charset = entry.name;

    if (headerRaw.publicId !== 0) {
      msg.publicId = WBXML_PUBLIC_ID[headerRaw.publicId];
    } else {
      msg.publicId = readStringTable(headerRaw.publicIdStr, msg.stringTable,
                                     WSP.WSP_WELL_KNOWN_CHARSETS[msg.charset].converter);
    }
    if (msg.publicId != appToken.publicId) {
      throw new Error("Public ID does not match.");
    }

    msg.version = ((headerRaw.version >> 4) + 1) + "." + (headerRaw.version & 0x0F);

    let decodeInfo = {
      charset: WSP.WSP_WELL_KNOWN_CHARSETS[msg.charset].converter,  // document character set
      stringTable: msg.stringTable                                  // document string table
    };
    msg.content = this.parseWbxml(data, decodeInfo, appToken);

    return msg;
  }
};

/**
 * Global Tokens
 *
 * @see WAP-192-WBXML-20010725-A, clause 7.1
 */
const WBXML_GLOBAL_TOKENS = (function () {
  let names = {};
  function add(number, coder) {
    let entry = {
      number: number,
      coder: coder,
    };
    names[number] = entry;
  }

  add(CODE_PAGE_SWITCH_TOKEN, WbxmlCodePageSwitch);
  add(TAG_END_TOKEN,          WbxmlEnd);
  add(INLINE_STRING_TOKEN,    WbxmlInlineString);
  add(STRING_TABLE_TOKEN,     WbxmlStringTable);
  add(OPAQUE_TOKEN,           WbxmlOpaque);

  return names;
})();

/**
 *  Pre-defined public IDs
 *
 * @see http://technical.openmobilealliance.org/tech/omna/omna-wbxml-public-docid.aspx
 */
const WBXML_PUBLIC_ID = (function () {
  let ids = {};
  function add(id, text) {
    ids[id] = text;
  }

  // Well Known Values
  add(0x01,     "UNKNOWN");
  add(0x02,     "-//WAPFORUM//DTD WML 1.0//EN");
  add(0x03,     "-//WAPFORUM//DTD WTA 1.0//EN");
  add(0x04,     "-//WAPFORUM//DTD WML 1.1//EN");
  add(0x05,     "-//WAPFORUM//DTD SI 1.0//EN");
  add(0x06,     "-//WAPFORUM//DTD SL 1.0//EN");
  add(0x07,     "-//WAPFORUM//DTD CO 1.0//EN");
  add(0x08,     "-//WAPFORUM//DTD CHANNEL 1.1//EN");
  add(0x09,     "-//WAPFORUM//DTD WML 1.2//EN");
  add(0x0A,     "-//WAPFORUM//DTD WML 1.3//EN");
  add(0x0B,     "-//WAPFORUM//DTD PROV 1.0//EN");
  add(0x0C,     "-//WAPFORUM//DTD WTA-WML 1.2//EN");
  add(0x0D,     "-//WAPFORUM//DTD EMN 1.0//EN");
  add(0x0E,     "-//OMA//DTD DRMREL 1.0//EN");
  add(0x0F,     "-//WIRELESSVILLAGE//DTD CSP 1.0//EN");
  add(0x10,     "-//WIRELESSVILLAGE//DTD CSP 1.1//EN");
  add(0x11,     "-//OMA//DTD WV-CSP 1.2//EN");
  add(0x12,     "-//OMA//DTD IMPS-CSP 1.3//EN");
  add(0x13,     "-//OMA//DRM 2.1//EN");
  add(0x14,     "-//OMA//SRM 1.0//EN");
  add(0x15,     "-//OMA//DCD 1.0//EN");
  add(0x16,     "-//OMA//DTD DS-DataObjectEmail 1.2//EN");
  add(0x17,     "-//OMA//DTD DS-DataObjectFolder 1.2//EN");
  add(0x18,     "-//OMA//DTD DS-DataObjectFile 1.2//EN");

  // Registered Values
  add(0x0FD1,   "-//SYNCML//DTD SyncML 1.0//EN");
  add(0x0FD2,   "-//SYNCML//DTD DevInf 1.0//EN");
  add(0x0FD3,   "-//SYNCML//DTD SyncML 1.1//EN");
  add(0x0FD4,   "-//SYNCML//DTD DevInf 1.1//EN");
  add(0x1100,   "-//PHONE.COM//DTD ALERT 1.0//EN");
  add(0x1101,   "-//PHONE.COM//DTD CACHE-OPERATION 1.0//EN");
  add(0x1102,   "-//PHONE.COM//DTD SIGNAL 1.0//EN");
  add(0x1103,   "-//PHONE.COM//DTD LIST 1.0//EN");
  add(0x1104,   "-//PHONE.COM//DTD LISTCMD 1.0//EN");
  add(0x1105,   "-//PHONE.COM//DTD CHANNEL 1.0//EN");
  add(0x1106,   "-//PHONE.COM//DTD MMC 1.0//EN");
  add(0x1107,   "-//PHONE.COM//DTD BEARER-CHOICE 1.0//EN");
  add(0x1108,   "-//PHONE.COM//DTD WML 1.1//EN");
  add(0x1109,   "-//PHONE.COM//DTD CHANNEL 1.1//EN");
  add(0x110A,   "-//PHONE.COM//DTD LIST 1.1//EN");
  add(0x110B,   "-//PHONE.COM//DTD LISTCMD 1.1//EN");
  add(0x110C,   "-//PHONE.COM//DTD MMC 1.1//EN");
  add(0x110D,   "-//PHONE.COM//DTD WML 1.3//EN");
  add(0x110E,   "-//PHONE.COM//DTD MMC 2.0//EN");
  add(0x1200,   "-//3GPP2.COM//DTD IOTA 1.0//EN");
  add(0x1201,   "-//SYNCML//DTD SyncML 1.2//EN");
  add(0x1202,   "-//SYNCML//DTD MetaInf 1.2//EN");
  add(0x1203,   "-//SYNCML//DTD DevInf 1.2//EN");
  add(0x1204,   "-//NOKIA//DTD LANDMARKS 1.0//EN");
  add(0x1205,   "-//SyncML//Schema SyncML 2.0//EN");
  add(0x1206,   "-//SyncML//Schema DevInf 2.0//EN");
  add(0x1207,   "-//OMA//DTD DRMREL 1.0//EN");

  return ids;
})();

this.EXPORTED_SYMBOLS = [
  // Parser
  "PduHelper",
];

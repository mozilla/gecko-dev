/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// WSP PDU Type Assignments
// @see WAP-230-WSP-20010705-a Appendix A. Assigned Numbers.
this.WSP_PDU_TYPE_PUSH = 0x06;

// Registered WDP Port Numbers
// @see WAP-259-WDP-20010614-a Appendix B. Port Number Definitions.
this.WDP_PORT_PUSH = 2948;

// Bearer Type Assignments
// @see WAP-259-WDP-20010614-a Appendix C. Network Bearer Table.
this.WDP_BEARER_GSM_SMS_GSM_MSISDN = 0x03;

this.ALL_CONST_SYMBOLS = undefined; // We want ALL_CONST_SYMBOLS to be exported.
this.ALL_CONST_SYMBOLS = Object.keys(this);

this.EXPORTED_SYMBOLS = ALL_CONST_SYMBOLS;

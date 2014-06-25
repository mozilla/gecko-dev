/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Helper function.
 */
function newUint8SupportOutgoingIndexWorker() {
  let worker = newWorker();
  let index = 4;          // index for read
  let buf = [0, 0, 0, 0]; // Preserved parcel size
  let context = worker.ContextPool._contexts[0];

  context.Buf.writeUint8 = function(value) {
    if (context.Buf.outgoingIndex >= buf.length) {
      buf.push(value);
    } else {
      buf[context.Buf.outgoingIndex] = value;
    }

    context.Buf.outgoingIndex++;
  };

  context.Buf.readUint8 = function() {
    return buf[index++];
  };

  context.Buf.seekIncoming = function(offset) {
    index += offset;
  };

  worker.debug = do_print;

  return worker;
}

// Test RIL requests related to STK.
/**
 * Verify if RIL.sendStkTerminalProfile be called.
 */
add_test(function test_if_send_stk_terminal_profile() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let profileSend = false;
  context.RIL.sendStkTerminalProfile = function(data) {
    profileSend = true;
  };

  let iccStatus = {
    gsmUmtsSubscriptionAppIndex: 0,
    apps: [{
      app_state: CARD_APPSTATE_READY,
      app_type: CARD_APPTYPE_USIM
    }],
  };
  worker.RILQUIRKS_SEND_STK_PROFILE_DOWNLOAD = false;

  context.RIL._processICCStatus(iccStatus);

  do_check_eq(profileSend, false);

  run_next_test();
});

/**
 * Verify RIL.sendStkTerminalProfile
 */
add_test(function test_send_stk_terminal_profile() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;

  ril.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);

  buf.seekIncoming(8);
  let profile = buf.readString();
  for (let i = 0; i < STK_SUPPORTED_TERMINAL_PROFILE.length; i++) {
    do_check_eq(parseInt(profile.substring(2 * i, 2 * i + 2), 16),
                STK_SUPPORTED_TERMINAL_PROFILE[i]);
  }

  run_next_test();
});

/**
 * Verify STK terminal response
 */
add_test(function test_stk_terminal_response() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    do_check_eq(this.readInt32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

    // Token : we don't care
    this.readInt32();

    // Data Size, 44 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
    //                      TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_RESULT_SIZE(3) +
    //                      TEXT LENGTH(10))
    do_check_eq(this.readInt32(), 44);

    // Command Details, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 3);
    do_check_eq(pduHelper.readHexOctet(), 0x01);
    do_check_eq(pduHelper.readHexOctet(), STK_CMD_PROVIDE_LOCAL_INFO);
    do_check_eq(pduHelper.readHexOctet(), STK_LOCAL_INFO_NNA);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Result
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_RESULT_OK);

    // Text
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_TEXT_STRING |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 8);
    do_check_eq(pduHelper.readHexOctet(), STK_TEXT_CODING_GSM_7BIT_PACKED);
    do_check_eq(pduHelper.readSeptetsToString(7, 0, PDU_NL_IDENTIFIER_DEFAULT,
                PDU_NL_IDENTIFIER_DEFAULT), "Mozilla");

    run_next_test();
  };

  let response = {
    command: {
      commandNumber: 0x01,
      typeOfCommand: STK_CMD_PROVIDE_LOCAL_INFO,
      commandQualifier: STK_LOCAL_INFO_NNA,
      options: {
        isPacked: true
      }
    },
    input: "Mozilla",
    resultCode: STK_RESULT_OK
  };
  context.RIL.sendStkTerminalResponse(response);
});

// Test ComprehensionTlvHelper

/**
 * Verify ComprehensionTlvHelper.writeLocationInfoTlv
 */
add_test(function test_write_location_info_tlv() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let tlvHelper = context.ComprehensionTlvHelper;

  // Test with 2-digit mnc, and gsmCellId obtained from UMTS network.
  let loc = {
    mcc: "466",
    mnc: "92",
    gsmLocationAreaCode : 10291,
    gsmCellId: 19072823
  };
  tlvHelper.writeLocationInfoTlv(loc);

  let tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  let length = pduHelper.readHexOctet();
  do_check_eq(length, 9);

  let mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  do_check_eq(mcc_mnc, "46692");

  let lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  do_check_eq(lac, 10291);

  let cellId = (pduHelper.readHexOctet() << 24) |
               (pduHelper.readHexOctet() << 16) |
               (pduHelper.readHexOctet() << 8)  |
               (pduHelper.readHexOctet());
  do_check_eq(cellId, 19072823);

  // Test with 1-digit mnc, and gsmCellId obtained from GSM network.
  loc = {
    mcc: "466",
    mnc: "02",
    gsmLocationAreaCode : 10291,
    gsmCellId: 65534
  };
  tlvHelper.writeLocationInfoTlv(loc);

  tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  length = pduHelper.readHexOctet();
  do_check_eq(length, 7);

  mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  do_check_eq(mcc_mnc, "46602");

  lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  do_check_eq(lac, 10291);

  cellId = (pduHelper.readHexOctet() << 8) | (pduHelper.readHexOctet());
  do_check_eq(cellId, 65534);

  // Test with 3-digit mnc, and gsmCellId obtained from GSM network.
  loc = {
    mcc: "466",
    mnc: "222",
    gsmLocationAreaCode : 10291,
    gsmCellId: 65534
  };
  tlvHelper.writeLocationInfoTlv(loc);

  tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  length = pduHelper.readHexOctet();
  do_check_eq(length, 7);

  mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  do_check_eq(mcc_mnc, "466222");

  lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  do_check_eq(lac, 10291);

  cellId = (pduHelper.readHexOctet() << 8) | (pduHelper.readHexOctet());
  do_check_eq(cellId, 65534);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.writeErrorNumber
 */
add_test(function test_write_disconnecting_cause() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let tlvHelper = context.ComprehensionTlvHelper;

  tlvHelper.writeCauseTlv(RIL_ERROR_TO_GECKO_ERROR[ERROR_GENERIC_FAILURE]);
  let tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_CAUSE | COMPREHENSIONTLV_FLAG_CR);
  let len = pduHelper.readHexOctet();
  do_check_eq(len, 2);  // We have one cause.
  let standard = pduHelper.readHexOctet();
  do_check_eq(standard, 0x60);
  let cause = pduHelper.readHexOctet();
  do_check_eq(cause, 0x80 | ERROR_GENERIC_FAILURE);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.getSizeOfLengthOctets
 */
add_test(function test_get_size_of_length_octets() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let tlvHelper = context.ComprehensionTlvHelper;

  let length = 0x70;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 1);

  length = 0x80;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 2);

  length = 0x180;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 3);

  length = 0x18000;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 4);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.writeLength
 */
add_test(function test_write_length() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let tlvHelper = context.ComprehensionTlvHelper;

  let length = 0x70;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), length);

  length = 0x80;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), 0x81);
  do_check_eq(pduHelper.readHexOctet(), length);

  length = 0x180;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), 0x82);
  do_check_eq(pduHelper.readHexOctet(), (length >> 8) & 0xff);
  do_check_eq(pduHelper.readHexOctet(), length & 0xff);

  length = 0x18000;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), 0x83);
  do_check_eq(pduHelper.readHexOctet(), (length >> 16) & 0xff);
  do_check_eq(pduHelper.readHexOctet(), (length >> 8) & 0xff);
  do_check_eq(pduHelper.readHexOctet(), length & 0xff);

  run_next_test();
});

// Test Proactive commands.
/**
 * Verify Proactive command helper : searchForNextTag
 */
add_test(function test_stk_proactive_command_search_next_tag() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let tag_test = [
    0xD0,
    0x3C,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x31,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x32,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x33,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x34,
    0x85, 0x0A, 0x61, 0x6C, 0x70, 0x68, 0x61, 0x20, 0x69, 0x64, 0x20, 0x35];

  for (let i = 0; i < tag_test.length; i++) {
    pduHelper.writeHexOctet(tag_test[i]);
  }

  let berTlv = berHelper.decode(tag_test.length);
  let iter = Iterator(berTlv.value);
  let tlv = stkHelper.searchForNextTag(COMPREHENSIONTLV_TAG_ALPHA_ID, iter);
  do_check_eq(tlv.value.identifier, "alpha id 1");

  tlv = stkHelper.searchForNextTag(COMPREHENSIONTLV_TAG_ALPHA_ID, iter);
  do_check_eq(tlv.value.identifier, "alpha id 2");

  tlv = stkHelper.searchForNextTag(COMPREHENSIONTLV_TAG_ALPHA_ID, iter);
  do_check_eq(tlv.value.identifier, "alpha id 3");

  tlv = stkHelper.searchForNextTag(COMPREHENSIONTLV_TAG_ALPHA_ID, iter);
  do_check_eq(tlv.value.identifier, "alpha id 4");

  tlv = stkHelper.searchForNextTag(COMPREHENSIONTLV_TAG_ALPHA_ID, iter);
  do_check_eq(tlv.value.identifier, "alpha id 5");

  run_next_test();
});

/**
 * Verify Proactive Command : Refresh
 */
add_test(function test_stk_proactive_command_refresh() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let refresh_1 = [
    0xD0,
    0x10,
    0x81, 0x03, 0x01, 0x01, 0x01,
    0x82, 0x02, 0x81, 0x82,
    0x92, 0x05, 0x01, 0x3F, 0x00, 0x2F, 0xE2];

  for (let i = 0; i < refresh_1.length; i++) {
    pduHelper.writeHexOctet(refresh_1[i]);
  }

  let berTlv = berHelper.decode(refresh_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x01);
  do_check_eq(tlv.value.commandQualifier, 0x01);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_FILE_LIST, ctlvs);
  do_check_eq(tlv.value.fileList, "3F002FE2");

  run_next_test();
});

/**
 * Verify Proactive Command : Play Tone
 */
add_test(function test_stk_proactive_command_play_tone() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let tone_1 = [
    0xD0,
    0x1B,
    0x81, 0x03, 0x01, 0x20, 0x00,
    0x82, 0x02, 0x81, 0x03,
    0x85, 0x09, 0x44, 0x69, 0x61, 0x6C, 0x20, 0x54, 0x6F, 0x6E, 0x65,
    0x8E, 0x01, 0x01,
    0x84, 0x02, 0x01, 0x05];

  for (let i = 0; i < tone_1.length; i++) {
    pduHelper.writeHexOctet(tone_1[i]);
  }

  let berTlv = berHelper.decode(tone_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x20);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
  do_check_eq(tlv.value.identifier, "Dial Tone");

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TONE, ctlvs);
  do_check_eq(tlv.value.tone, STK_TONE_TYPE_DIAL_TONE);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_DURATION, ctlvs);
  do_check_eq(tlv.value.timeUnit, STK_TIME_UNIT_SECOND);
  do_check_eq(tlv.value.timeInterval, 5);

  run_next_test();
});

/**
 * Verify Proactive Command : Poll Interval
 */
add_test(function test_stk_proactive_command_poll_interval() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let poll_1 = [
    0xD0,
    0x0D,
    0x81, 0x03, 0x01, 0x03, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x84, 0x02, 0x01, 0x14];

  for (let i = 0; i < poll_1.length; i++) {
    pduHelper.writeHexOctet(poll_1[i]);
  }

  let berTlv = berHelper.decode(poll_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x03);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_DURATION, ctlvs);
  do_check_eq(tlv.value.timeUnit, STK_TIME_UNIT_SECOND);
  do_check_eq(tlv.value.timeInterval, 0x14);

  run_next_test();
});

/**
 * Verify Proactive Command: Display Text
 */
add_test(function test_read_septets_to_string() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let display_text_1 = [
    0xd0,
    0x28,
    0x81, 0x03, 0x01, 0x21, 0x80,
    0x82, 0x02, 0x81, 0x02,
    0x0d, 0x1d, 0x00, 0xd3, 0x30, 0x9b, 0xfc, 0x06, 0xc9, 0x5c, 0x30, 0x1a,
    0xa8, 0xe8, 0x02, 0x59, 0xc3, 0xec, 0x34, 0xb9, 0xac, 0x07, 0xc9, 0x60,
    0x2f, 0x58, 0xed, 0x15, 0x9b, 0xb9, 0x40,
  ];

  for (let i = 0; i < display_text_1.length; i++) {
    pduHelper.writeHexOctet(display_text_1[i]);
  }

  let berTlv = berHelper.decode(display_text_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
  do_check_eq(tlv.value.textString, "Saldo 2.04 E. Validez 20/05/13. ");

  run_next_test();
});

/**
 * Verify Proactive Command: Set Up Event List.
 */
add_test(function test_stk_proactive_command_event_list() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let event_1 = [
    0xD0,
    0x0F,
    0x81, 0x03, 0x01, 0x05, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x99, 0x04, 0x00, 0x01, 0x02, 0x03];

  for (let i = 0; i < event_1.length; i++) {
    pduHelper.writeHexOctet(event_1[i]);
  }

  let berTlv = berHelper.decode(event_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x05);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_EVENT_LIST, ctlvs);
  do_check_eq(Array.isArray(tlv.value.eventList), true);
  for (let i = 0; i < tlv.value.eventList.length; i++) {
    do_check_eq(tlv.value.eventList[i], i);
  }

  run_next_test();
});

/**
 * Verify Proactive Command : Get Input
 */
add_test(function test_stk_proactive_command_get_input() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;
  let stkCmdHelper = context.StkCommandParamsFactory;

  let get_input_1 = [
    0xD0,
    0x1E,
    0x81, 0x03, 0x01, 0x23, 0x8F,
    0x82, 0x02, 0x81, 0x82,
    0x8D, 0x05, 0x04, 0x54, 0x65, 0x78, 0x74,
    0x91, 0x02, 0x01, 0x10,
    0x17, 0x08, 0x04, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74];

  for (let i = 0; i < get_input_1.length; i++) {
    pduHelper.writeHexOctet(get_input_1[i]);
  }

  let berTlv = berHelper.decode(get_input_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_GET_INPUT);

  let input = stkCmdHelper.createParam(tlv.value, ctlvs);
  do_check_eq(input.text, "Text");
  do_check_eq(input.isAlphabet, true);
  do_check_eq(input.isUCS2, true);
  do_check_eq(input.hideInput, true);
  do_check_eq(input.isPacked, true);
  do_check_eq(input.isHelpAvailable, true);
  do_check_eq(input.minLength, 0x01);
  do_check_eq(input.maxLength, 0x10);
  do_check_eq(input.defaultText, "Default");

  let get_input_2 = [
    0xD0,
    0x11,
    0x81, 0x03, 0x01, 0x23, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x8D, 0x00,
    0x91, 0x02, 0x01, 0x10,
    0x17, 0x00];

  for (let i = 0; i < get_input_2.length; i++) {
    pduHelper.writeHexOctet(get_input_2[i]);
  }

  berTlv = berHelper.decode(get_input_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_GET_INPUT);

  input = stkCmdHelper.createParam(tlv.value, ctlvs);
  do_check_eq(input.text, null);
  do_check_eq(input.minLength, 0x01);
  do_check_eq(input.maxLength, 0x10);
  do_check_eq(input.defaultText, null);

  run_next_test();
});

/**
 * Verify Proactive Command : More Time
 */
add_test(function test_stk_proactive_command_more_time() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  let more_time_1 = [
    0xD0,
    0x09,
    0x81, 0x03, 0x01, 0x02, 0x00,
    0x82, 0x02, 0x81, 0x82];

  for(let i = 0 ; i < more_time_1.length; i++) {
    pduHelper.writeHexOctet(more_time_1[i]);
  }

  let berTlv = berHelper.decode(more_time_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_MORE_TIME);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  run_next_test();
});

/**
 * Verify Proactive Command : Select Item
 */
add_test(function test_stk_proactive_command_select_item() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;
  let stkFactory = context.StkCommandParamsFactory;

  let select_item_1 = [
    0xD0,
    0x33,
    0x81, 0x03, 0x01, 0x24, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
    0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
    0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
    0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
    0x18, 0x03, 0x10, 0x15, 0x20,
    0x90, 0x01, 0x01
  ];

  for(let i = 0 ; i < select_item_1.length; i++) {
    pduHelper.writeHexOctet(select_item_1[i]);
  }

  let berTlv = berHelper.decode(select_item_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_SELECT_ITEM);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  let menu = stkFactory.createParam(tlv.value, ctlvs);
  do_check_eq(menu.title, "Title");
  do_check_eq(menu.items[0].identifier, 1);
  do_check_eq(menu.items[0].text, "item 1");
  do_check_eq(menu.items[1].identifier, 2);
  do_check_eq(menu.items[1].text, "item 2");
  do_check_eq(menu.items[2].identifier, 3);
  do_check_eq(menu.items[2].text, "item 3");
  do_check_eq(menu.nextActionList[0], STK_CMD_SET_UP_CALL);
  do_check_eq(menu.nextActionList[1], STK_CMD_LAUNCH_BROWSER);
  do_check_eq(menu.nextActionList[2], STK_CMD_PLAY_TONE);
  do_check_eq(menu.defaultItem, 0x00);

  let select_item_2 = [
    0xD0,
    0x33,
    0x81, 0x03, 0x01, 0x24, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
    0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
    0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
    0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
    0x18, 0x03, 0x00, 0x15, 0x81,
    0x90, 0x01, 0x03
  ];

  for(let i = 0 ; i < select_item_2.length; i++) {
    pduHelper.writeHexOctet(select_item_2[i]);
  }

  berTlv = berHelper.decode(select_item_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_SELECT_ITEM);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  menu = stkFactory.createParam(tlv.value, ctlvs);
  do_check_eq(menu.title, "Title");
  do_check_eq(menu.items[0].identifier, 1);
  do_check_eq(menu.items[0].text, "item 1");
  do_check_eq(menu.items[1].identifier, 2);
  do_check_eq(menu.items[1].text, "item 2");
  do_check_eq(menu.items[2].identifier, 3);
  do_check_eq(menu.items[2].text, "item 3");
  do_check_eq(menu.nextActionList[0], STK_NEXT_ACTION_NULL);
  do_check_eq(menu.nextActionList[1], STK_CMD_LAUNCH_BROWSER);
  do_check_eq(menu.nextActionList[2], STK_NEXT_ACTION_END_PROACTIVE_SESSION);
  do_check_eq(menu.defaultItem, 0x02);

  run_next_test();
});

/**
 * Verify Proactive Command : Set Up Menu
 */
add_test(function test_stk_proactive_command_set_up_menu() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;
  let stkFactory = context.StkCommandParamsFactory;

  let set_up_menu_1 = [
    0xD0,
    0x30,
    0x81, 0x03, 0x01, 0x25, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
    0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
    0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
    0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
    0x18, 0x03, 0x10, 0x15, 0x20
  ];

  for(let i = 0 ; i < set_up_menu_1.length; i++) {
    pduHelper.writeHexOctet(set_up_menu_1[i]);
  }

  let berTlv = berHelper.decode(set_up_menu_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_SET_UP_MENU);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  let menu = stkFactory.createParam(tlv.value, ctlvs);
  do_check_eq(menu.title, "Title");
  do_check_eq(menu.items[0].identifier, 1);
  do_check_eq(menu.items[0].text, "item 1");
  do_check_eq(menu.items[1].identifier, 2);
  do_check_eq(menu.items[1].text, "item 2");
  do_check_eq(menu.items[2].identifier, 3);
  do_check_eq(menu.items[2].text, "item 3");
  do_check_eq(menu.nextActionList[0], STK_CMD_SET_UP_CALL);
  do_check_eq(menu.nextActionList[1], STK_CMD_LAUNCH_BROWSER);
  do_check_eq(menu.nextActionList[2], STK_CMD_PLAY_TONE);

  let set_up_menu_2 = [
    0xD0,
    0x30,
    0x81, 0x03, 0x01, 0x25, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x05, 0x54, 0x69, 0x74, 0x6C, 0x65,
    0x8F, 0x07, 0x01, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x31,
    0x8F, 0x07, 0x02, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x32,
    0x8F, 0x07, 0x03, 0x69, 0x74, 0x65, 0x6D, 0x20, 0x33,
    0x18, 0x03, 0x81, 0x00, 0x00
  ];

  for(let i = 0 ; i < set_up_menu_2.length; i++) {
    pduHelper.writeHexOctet(set_up_menu_2[i]);
  }

  berTlv = berHelper.decode(set_up_menu_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_SET_UP_MENU);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  let menu = stkFactory.createParam(tlv.value, ctlvs);
  do_check_eq(menu.title, "Title");
  do_check_eq(menu.items[0].identifier, 1);
  do_check_eq(menu.items[0].text, "item 1");
  do_check_eq(menu.items[1].identifier, 2);
  do_check_eq(menu.items[1].text, "item 2");
  do_check_eq(menu.items[2].identifier, 3);
  do_check_eq(menu.items[2].text, "item 3");
  do_check_eq(menu.nextActionList[0], STK_NEXT_ACTION_END_PROACTIVE_SESSION);
  do_check_eq(menu.nextActionList[1], STK_NEXT_ACTION_NULL);
  do_check_eq(menu.nextActionList[2], STK_NEXT_ACTION_NULL);

  run_next_test();
});

/**
 * Verify Proactive Command : Set Up Call
 */
add_test(function test_stk_proactive_command_set_up_call() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;
  let cmdFactory = context.StkCommandParamsFactory;

  let set_up_call_1 = [
    0xD0,
    0x29,
    0x81, 0x03, 0x01, 0x10, 0x04,
    0x82, 0x02, 0x81, 0x82,
    0x05, 0x0A, 0x44, 0x69, 0x73, 0x63, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74,
    0x86, 0x09, 0x81, 0x10, 0x32, 0x04, 0x21, 0x43, 0x65, 0x1C, 0x2C,
    0x05, 0x07, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65];

  for (let i = 0 ; i < set_up_call_1.length; i++) {
    pduHelper.writeHexOctet(set_up_call_1[i]);
  }

  let berTlv = berHelper.decode(set_up_call_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_SET_UP_CALL);

  let setupCall = cmdFactory.createParam(tlv.value, ctlvs);
  do_check_eq(setupCall.address, "012340123456,1,2");
  do_check_eq(setupCall.confirmMessage, "Disconnect");
  do_check_eq(setupCall.callMessage, "Message");

  run_next_test();
});

/**
 * Verify Proactive Command : Timer Management
 */
add_test(function test_stk_proactive_command_timer_management() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  // Timer Management - Start
  let timer_management_1 = [
    0xD0,
    0x11,
    0x81, 0x03, 0x01, 0x27, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0xA4, 0x01, 0x01,
    0xA5, 0x03, 0x10, 0x20, 0x30
  ];

  for(let i = 0 ; i < timer_management_1.length; i++) {
    pduHelper.writeHexOctet(timer_management_1[i]);
  }

  let berTlv = berHelper.decode(timer_management_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_TIMER_MANAGEMENT);
  do_check_eq(tlv.value.commandQualifier, STK_TIMER_START);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER, ctlvs);
  do_check_eq(tlv.value.timerId, 0x01);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TIMER_VALUE, ctlvs);
  do_check_eq(tlv.value.timerValue, (0x01 * 60 * 60) + (0x02 * 60) + 0x03);

  // Timer Management - Deactivate
  let timer_management_2 = [
    0xD0,
    0x0C,
    0x81, 0x03, 0x01, 0x27, 0x01,
    0x82, 0x02, 0x81, 0x82,
    0xA4, 0x01, 0x01
  ];

  for(let i = 0 ; i < timer_management_2.length; i++) {
    pduHelper.writeHexOctet(timer_management_2[i]);
  }

  berTlv = berHelper.decode(timer_management_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_TIMER_MANAGEMENT);
  do_check_eq(tlv.value.commandQualifier, STK_TIMER_DEACTIVATE);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER, ctlvs);
  do_check_eq(tlv.value.timerId, 0x01);

  run_next_test();
});

/**
 * Verify Proactive Command : Provide Local Information
 */
add_test(function test_stk_proactive_command_provide_local_information() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  // Verify IMEI
  let local_info_1 = [
    0xD0,
    0x09,
    0x81, 0x03, 0x01, 0x26, 0x01,
    0x82, 0x02, 0x81, 0x82];

  for (let i = 0; i < local_info_1.length; i++) {
    pduHelper.writeHexOctet(local_info_1[i]);
  }

  let berTlv = berHelper.decode(local_info_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_PROVIDE_LOCAL_INFO);
  do_check_eq(tlv.value.commandQualifier, STK_LOCAL_INFO_IMEI);

  // Verify Date and Time Zone
  let local_info_2 = [
    0xD0,
    0x09,
    0x81, 0x03, 0x01, 0x26, 0x03,
    0x82, 0x02, 0x81, 0x82];

  for (let i = 0; i < local_info_2.length; i++) {
    pduHelper.writeHexOctet(local_info_2[i]);
  }

  berTlv = berHelper.decode(local_info_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_PROVIDE_LOCAL_INFO);
  do_check_eq(tlv.value.commandQualifier, STK_LOCAL_INFO_DATE_TIME_ZONE);

  run_next_test();
});

/**
 * Verify Proactive command : BIP Messages
 */
add_test(function test_stk_proactive_command_open_channel() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;
  let stkHelper = context.StkProactiveCmdHelper;

  // Open Channel
  let open_channel = [
    0xD0,
    0x0F,
    0x81, 0x03, 0x01, 0x40, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x04, 0x4F, 0x70, 0x65, 0x6E //alpha id: "Open"
  ];

  for (let i = 0; i < open_channel.length; i++) {
    pduHelper.writeHexOctet(open_channel[i]);
  }

  let berTlv = berHelper.decode(open_channel.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_OPEN_CHANNEL);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
  do_check_eq(tlv.value.identifier, "Open");

  // Close Channel
  let close_channel = [
    0xD0,
    0x10,
    0x81, 0x03, 0x01, 0x41, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x05, 0x43, 0x6C, 0x6F, 0x73, 0x65 //alpha id: "Close"
  ];

  for (let i = 0; i < close_channel.length; i++) {
    pduHelper.writeHexOctet(close_channel[i]);
  }

  berTlv = berHelper.decode(close_channel.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_CLOSE_CHANNEL);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
  do_check_eq(tlv.value.identifier, "Close");

  // Receive Data
  let receive_data = [
    0XD0,
    0X12,
    0x81, 0x03, 0x01, 0x42, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x07, 0x52, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65 //alpha id: "Receive"
  ];

  for (let i = 0; i < receive_data.length; i++) {
    pduHelper.writeHexOctet(receive_data[i]);
  }

  berTlv = berHelper.decode(receive_data.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_RECEIVE_DATA);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
  do_check_eq(tlv.value.identifier, "Receive");

  // Send Data
  let send_data = [
    0xD0,
    0x0F,
    0x81, 0x03, 0x01, 0x43, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x85, 0x04, 0x53, 0x65, 0x6E, 0x64 //alpha id: "Send"
  ];

  for (let i = 0; i < send_data.length; i++) {
    pduHelper.writeHexOctet(send_data[i]);
  }

  berTlv = berHelper.decode(send_data.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_SEND_DATA);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
  do_check_eq(tlv.value.identifier, "Send");

  run_next_test();
});

/**
 * Verify Event Download Command : Location Status
 */
add_test(function test_stk_event_download_location_status() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    do_check_eq(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 42 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_EVENT_LIST_SIZE(3) +
    //                      TLV_LOCATION_STATUS_SIZE(3) +
    //                      TLV_LOCATION_INFO_GSM_SIZE(9))
    do_check_eq(this.readInt32(), 42);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 19 = TLV_DEVICE_ID_SIZE(4) +
    //                  TLV_EVENT_LIST_SIZE(3) +
    //                  TLV_LOCATION_STATUS_SIZE(3) +
    //                  TLV_LOCATION_INFO_GSM_SIZE(9)
    do_check_eq(pduHelper.readHexOctet(), 19);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_LOCATION_STATUS);

    // Location Status, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LOCATION_STATUS |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_SERVICE_STATE_NORMAL);

    // Location Info, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LOCATION_INFO |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 7);

    do_check_eq(pduHelper.readHexOctet(), 0x21); // MCC + MNC
    do_check_eq(pduHelper.readHexOctet(), 0x63);
    do_check_eq(pduHelper.readHexOctet(), 0x54);
    do_check_eq(pduHelper.readHexOctet(), 0); // LAC
    do_check_eq(pduHelper.readHexOctet(), 0);
    do_check_eq(pduHelper.readHexOctet(), 0); // Cell ID
    do_check_eq(pduHelper.readHexOctet(), 0);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_LOCATION_STATUS,
    locationStatus: STK_SERVICE_STATE_NORMAL,
    locationInfo: {
      mcc: "123",
      mnc: "456",
      gsmLocationAreaCode: 0,
      gsmCellId: 0
    }
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

// Test Event Download commands.

/**
 * Verify Event Download Command : Language Selection
 */
add_test(function test_stk_event_download_language_selection() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;

  buf.sendParcel = function() {
    // Type
    do_check_eq(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 26 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_EVENT_LIST_SIZE(3) +
    //                      TLV_LANGUAGE(4))
    do_check_eq(this.readInt32(), 26);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 19 = TLV_DEVICE_ID_SIZE(4) +
    //                  TLV_EVENT_LIST_SIZE(3) +
    //                  TLV_LANGUAGE(4)
    do_check_eq(pduHelper.readHexOctet(), 11);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_LANGUAGE_SELECTION);

    // Language, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LANGUAGE);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(iccHelper.read8BitUnpackedToString(2), "zh");

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_LANGUAGE_SELECTION,
    language: "zh"
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify Event Download Command : User Activity
 */
add_test(function test_stk_event_download_user_activity() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    do_check_eq(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 18 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3))
    do_check_eq(this.readInt32(), 18);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 7 = TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3)
    do_check_eq(pduHelper.readHexOctet(), 7);

    // Device Identities, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_USER_ACTIVITY);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_USER_ACTIVITY
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify Event Download Command : Idle Screen Available
 */
add_test(function test_stk_event_download_idle_screen_available() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    do_check_eq(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 18 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3))
    do_check_eq(this.readInt32(), 18);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 7 = TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3)
    do_check_eq(pduHelper.readHexOctet(), 7);

    // Device Identities, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_DISPLAY);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify Event Downloaded Command :Browser Termination
 */
add_test(function test_stk_event_download_browser_termination() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let pduHelper = context.GsmPDUHelper;

  buf.sendParcel = function() {
    // Type
    do_check_eq(this.readInt32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readInt32();

    // Data Size, 24 = 2 * ( 2+TLV_DEVICE_ID(4)+TLV_EVENT_LIST_SIZE(3)
    //                        +TLV_BROWSER_TERMINATION_CAUSE(3) )
    do_check_eq(this.readInt32(), 24);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 10 = TLV_DEVICE_ID(4)+TLV_EVENT_LIST_SIZE(3)
    //                  ++TLV_BROWSER_TERMINATION_CAUSE(3)
    do_check_eq(pduHelper.readHexOctet(), 10);

    // Device Identities, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_BROWSER_TERMINATION);

    // Browser Termination Case, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_BROWSER_TERMINATION_CAUSE |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_BROWSER_TERMINATION_CAUSE_USER);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_BROWSER_TERMINATION,
    terminationCause: STK_BROWSER_TERMINATION_CAUSE_USER
  };
  context.RIL.sendStkEventDownload({
    event: event
  });
});

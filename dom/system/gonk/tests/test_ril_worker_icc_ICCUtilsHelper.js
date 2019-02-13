/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Verify ICCUtilsHelper.isICCServiceAvailable.
 */
add_test(function test_is_icc_service_available() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ICCUtilsHelper = context.ICCUtilsHelper;
  let RIL = context.RIL;

  function test_table(sst, geckoService, simEnabled, usimEnabled) {
    RIL.iccInfoPrivate.sst = sst;
    RIL.appType = CARD_APPTYPE_SIM;
    equal(ICCUtilsHelper.isICCServiceAvailable(geckoService), simEnabled);
    RIL.appType = CARD_APPTYPE_USIM;
    equal(ICCUtilsHelper.isICCServiceAvailable(geckoService), usimEnabled);
  }

  test_table([0x08], "ADN", true, false);
  test_table([0x08], "FDN", false, false);
  test_table([0x08], "SDN", false, true);

  run_next_test();
});

/**
 * Verify ICCUtilsHelper.isGsm8BitAlphabet
 */
add_test(function test_is_gsm_8bit_alphabet() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ICCUtilsHelper = context.ICCUtilsHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  equal(ICCUtilsHelper.isGsm8BitAlphabet(langTable), true);
  equal(ICCUtilsHelper.isGsm8BitAlphabet(langShiftTable), true);
  equal(ICCUtilsHelper.isGsm8BitAlphabet("\uaaaa"), false);

  run_next_test();
});

/**
 * Verify ICCUtilsHelper.parsePbrTlvs
 */
add_test(function test_parse_pbr_tlvs() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;

  let pbrTlvs = [
    {tag: ICC_USIM_TYPE1_TAG,
     length: 0x0F,
     value: [{tag: ICC_USIM_EFADN_TAG,
              length: 0x03,
              value: [0x4F, 0x3A, 0x02]},
             {tag: ICC_USIM_EFIAP_TAG,
              length: 0x03,
              value: [0x4F, 0x25, 0x01]},
             {tag: ICC_USIM_EFPBC_TAG,
              length: 0x03,
              value: [0x4F, 0x09, 0x04]}]
    },
    {tag: ICC_USIM_TYPE2_TAG,
     length: 0x05,
     value: [{tag: ICC_USIM_EFEMAIL_TAG,
              length: 0x03,
              value: [0x4F, 0x50, 0x0B]},
             {tag: ICC_USIM_EFANR_TAG,
              length: 0x03,
              value: [0x4F, 0x11, 0x02]},
             {tag: ICC_USIM_EFANR_TAG,
              length: 0x03,
              value: [0x4F, 0x12, 0x03]}]
    },
    {tag: ICC_USIM_TYPE3_TAG,
     length: 0x0A,
     value: [{tag: ICC_USIM_EFCCP1_TAG,
              length: 0x03,
              value: [0x4F, 0x3D, 0x0A]},
             {tag: ICC_USIM_EFEXT1_TAG,
              length: 0x03,
              value: [0x4F, 0x4A, 0x03]}]
    },
  ];

  let pbr = context.ICCUtilsHelper.parsePbrTlvs(pbrTlvs);
  equal(pbr.adn.fileId, 0x4F3a);
  equal(pbr.iap.fileId, 0x4F25);
  equal(pbr.pbc.fileId, 0x4F09);
  equal(pbr.email.fileId, 0x4F50);
  equal(pbr.anr0.fileId, 0x4f11);
  equal(pbr.anr1.fileId, 0x4f12);
  equal(pbr.ccp1.fileId, 0x4F3D);
  equal(pbr.ext1.fileId, 0x4F4A);

  run_next_test();
});

/**
 * Verify MCC and MNC parsing
 */
add_test(function test_mcc_mnc_parsing() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.ICCUtilsHelper;

  function do_test(imsi, mncLength, expectedMcc, expectedMnc) {
    let result = helper.parseMccMncFromImsi(imsi, mncLength);

    if (!imsi) {
      equal(result, null);
      return;
    }

    equal(result.mcc, expectedMcc);
    equal(result.mnc, expectedMnc);
  }

  // Test the imsi is null.
  do_test(null, null, null, null);

  // Test MCC is Taiwan
  do_test("466923202422409", 0x02, "466", "92");
  do_test("466923202422409", 0x03, "466", "923");
  do_test("466923202422409", null, "466", "92");

  // Test MCC is US
  do_test("310260542718417", 0x02, "310", "26");
  do_test("310260542718417", 0x03, "310", "260");
  do_test("310260542718417", null, "310", "260");

  run_next_test();
});

add_test(function test_get_network_name_from_icc() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let ICCUtilsHelper = context.ICCUtilsHelper;

  function testGetNetworkNameFromICC(operatorData, expectedResult) {
    let result = ICCUtilsHelper.getNetworkNameFromICC(operatorData.mcc,
                                                      operatorData.mnc,
                                                      operatorData.lac);

    if (expectedResult == null) {
      equal(result, expectedResult);
    } else {
      equal(result.fullName, expectedResult.longName);
      equal(result.shortName, expectedResult.shortName);
    }
  }

  // Before EF_OPL and EF_PNN have been loaded.
  testGetNetworkNameFromICC({mcc: "123", mnc: "456", lac: 0x1000}, null);
  testGetNetworkNameFromICC({mcc: "321", mnc: "654", lac: 0x2000}, null);

  // Set HPLMN
  RIL.iccInfo.mcc = "123";
  RIL.iccInfo.mnc = "456";

  RIL.voiceRegistrationState = {
    cell: {
      gsmLocationAreaCode: 0x1000
    }
  };
  RIL.operator = {};

  // Set EF_PNN
  RIL.iccInfoPrivate = {
    PNN: [
      {"fullName": "PNN1Long", "shortName": "PNN1Short"},
      {"fullName": "PNN2Long", "shortName": "PNN2Short"},
      {"fullName": "PNN3Long", "shortName": "PNN3Short"},
      {"fullName": "PNN4Long", "shortName": "PNN4Short"},
      {"fullName": "PNN5Long", "shortName": "PNN5Short"},
      {"fullName": "PNN6Long", "shortName": "PNN6Short"},
      {"fullName": "PNN7Long", "shortName": "PNN7Short"},
      {"fullName": "PNN8Long", "shortName": "PNN8Short"}
    ]
  };

  // EF_OPL isn't available
  ICCUtilsHelper.isICCServiceAvailable = function fakeIsICCServiceAvailable(service) {
    return false;
  };

  // EF_OPL isn't available and current isn't in HPLMN,
  testGetNetworkNameFromICC({mcc: "321", mnc: "654", lac: 0x1000}, null);

  // EF_OPL isn't available and current is in HPLMN,
  // the first record of PNN should be returned.
  testGetNetworkNameFromICC({mcc: "123", mnc: "456", lac: 0x1000},
                            {longName: "PNN1Long", shortName: "PNN1Short"});

  // EF_OPL is available
  ICCUtilsHelper.isICCServiceAvailable = function fakeIsICCServiceAvailable(service) {
    return service === "OPL";
  };

  // Set EF_OPL
  RIL.iccInfoPrivate.OPL = [
    {
      "mcc": "123",
      "mnc": "456",
      "lacTacStart": 0,
      "lacTacEnd": 0xFFFE,
      "pnnRecordId": 4
    },
    {
      "mcc": "321",
      "mnc": "654",
      "lacTacStart": 0,
      "lacTacEnd": 0x0010,
      "pnnRecordId": 3
    },
    {
      "mcc": "321",
      "mnc": "654",
      "lacTacStart": 0x0100,
      "lacTacEnd": 0x1010,
      "pnnRecordId": 2
    },
    {
      "mcc": ";;;",
      "mnc": "01",
      "lacTacStart": 0,
      "lacTacEnd": 0xFFFE,
      "pnnRecordId": 5
    },
    {
      "mcc": "00;",
      "mnc": "02",
      "lacTacStart": 0,
      "lacTacEnd": 0xFFFE,
      "pnnRecordId": 6
    },
    {
      "mcc": "001",
      "mnc": ";;",
      "lacTacStart": 0,
      "lacTacEnd": 0xFFFE,
      "pnnRecordId": 7
    },
    {
      "mcc": "002",
      "mnc": "0;",
      "lacTacStart": 0,
      "lacTacEnd": 0xFFFE,
      "pnnRecordId": 8
    }
  ];

  // Both EF_PNN and EF_OPL are presented, and current PLMN is HPLMN,
  testGetNetworkNameFromICC({mcc: "123", mnc: "456", lac: 0x1000},
                            {longName: "PNN4Long", shortName: "PNN4Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the second PNN record.
  testGetNetworkNameFromICC({mcc: "321", mnc: "654", lac: 0x1000},
                            {longName: "PNN2Long", shortName: "PNN2Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the thrid PNN record.
  testGetNetworkNameFromICC({mcc: "321", mnc: "654", lac: 0x0001},
                            {longName: "PNN3Long", shortName: "PNN3Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the 5th PNN record after wild char (ie: ';') handling.
  testGetNetworkNameFromICC({mcc: "001", mnc: "01", lac: 0x0001},
                            {longName: "PNN5Long", shortName: "PNN5Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the 6th PNN record after wild char (ie: ';') handling.
  testGetNetworkNameFromICC({mcc: "001", mnc: "02", lac: 0x0001},
                            {longName: "PNN6Long", shortName: "PNN6Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the 7th PNN record after wild char (ie: ';') handling.
  testGetNetworkNameFromICC({mcc: "001", mnc: "03", lac: 0x0001},
                            {longName: "PNN7Long", shortName: "PNN7Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the 8th PNN record after wild char (ie: ';') handling.
  testGetNetworkNameFromICC({mcc: "002", mnc: "03", lac: 0x0001},
                            {longName: "PNN8Long", shortName: "PNN8Short"});

  run_next_test();
});

/**
 * Verify ICCUtilsHelper.isCphsServiceAvailable.
 */
add_test(function test_is_cphs_service_available() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ICCUtilsHelper = context.ICCUtilsHelper;
  let RIL = context.RIL;
  RIL.iccInfoPrivate.cphsSt = Uint8Array(2);

  function test_table(cphsSt, geckoService) {
    RIL.iccInfoPrivate.cphsSt.set(cphsSt);

    for (let service in GECKO_ICC_SERVICES.cphs) {
      equal(ICCUtilsHelper.isCphsServiceAvailable(service),
                  geckoService == service);
    }
  }

  test_table([0x03, 0x00], "CSP");
  test_table([0x0C, 0x00], "SST");
  test_table([0x30, 0x00], "MBN");
  test_table([0xC0, 0x00], "ONSF");
  test_table([0x00, 0x03], "INFO_NUM");

  run_next_test();
});

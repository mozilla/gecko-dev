/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

let NS = {};
subscriptLoader.loadSubScript("resource://gre/modules/DialNumberUtils.jsm", NS);

function run_test() {
  run_next_test();
}

function parseMMI(mmiString) {
  return NS.DialNumberUtils.parseMMI(mmiString);
}

add_test(function test_parseMMI_empty() {
  let mmi = parseMMI("");

  equal(mmi, null);

  run_next_test();
});

add_test(function test_parseMMI_undefined() {
  let mmi = parseMMI();

  equal(mmi, null);

  run_next_test();
});

add_test(function test_parseMMI_dial_string() {
  let mmi = parseMMI("12345");

  equal(mmi, null);

  run_next_test();
});

add_test(function test_parseMMI_USSD_without_asterisk_prefix() {
  let mmi = parseMMI("123#");

  equal(mmi, null);

  run_next_test();
});

add_test(function test_parseMMI_USSD() {
  let mmi = parseMMI("*123#");

  equal(mmi.fullMMI, "*123#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, undefined);
  equal(mmi.sib, undefined);
  equal(mmi.sic, undefined);
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_USSD_with_two_sharps() {
  let mmi = parseMMI("*225#4384903113430962#");

  equal(mmi, null);

  run_next_test();
});

add_test(function test_parseMMI_sia() {
  let mmi = parseMMI("*123*1#");

  equal(mmi.fullMMI, "*123*1#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, "1");
  equal(mmi.sib, undefined);
  equal(mmi.sic, undefined);
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_sib() {
  let mmi = parseMMI("*123**1#");

  equal(mmi.fullMMI, "*123**1#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, "");
  equal(mmi.sib, "1");
  equal(mmi.sic, undefined);
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_sic() {
  let mmi = parseMMI("*123***1#");

  equal(mmi.fullMMI, "*123***1#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, "");
  equal(mmi.sib, "");
  equal(mmi.sic, "1");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_sia_sib() {
  let mmi = parseMMI("*123*1*1#");

  equal(mmi.fullMMI, "*123*1*1#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, "1");
  equal(mmi.sib, "1");
  equal(mmi.sic, undefined);
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_sia_sic() {
  let mmi = parseMMI("*123*1**1#");

  equal(mmi.fullMMI, "*123*1**1#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, "1");
  equal(mmi.sib, "");
  equal(mmi.sic, "1");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_sib_sic() {
  let mmi = parseMMI("*123**1*1#");

  equal(mmi.fullMMI, "*123**1*1#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, "");
  equal(mmi.sib, "1");
  equal(mmi.sic, "1");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_pwd() {
  let mmi = parseMMI("*123****1#");

  equal(mmi.fullMMI, "*123****1#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, "");
  equal(mmi.sib, "");
  equal(mmi.sic, "");
  equal(mmi.pwd, "1");
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_dial_number() {
  let mmi = parseMMI("*123#345");

  equal(mmi.fullMMI, "*123#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "123");
  equal(mmi.sia, undefined);
  equal(mmi.sib, undefined);
  equal(mmi.sic, undefined);
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, "345");

  run_next_test();
});


/**
 * MMI procedures tests
 */

add_test(function test_parseMMI_activation() {
  let mmi = parseMMI("*00*12*34*56#");

  equal(mmi.fullMMI, "*00*12*34*56#");
  equal(mmi.procedure, MMI_PROCEDURE_ACTIVATION);
  equal(mmi.serviceCode, "00");
  equal(mmi.sia, "12");
  equal(mmi.sib, "34");
  equal(mmi.sic, "56");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_deactivation() {
  let mmi = parseMMI("#00*12*34*56#");

  equal(mmi.fullMMI, "#00*12*34*56#");
  equal(mmi.procedure, MMI_PROCEDURE_DEACTIVATION);
  equal(mmi.serviceCode, "00");
  equal(mmi.sia, "12");
  equal(mmi.sib, "34");
  equal(mmi.sic, "56");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_interrogation() {
  let mmi = parseMMI("*#00*12*34*56#");

  equal(mmi.fullMMI, "*#00*12*34*56#");
  equal(mmi.procedure, MMI_PROCEDURE_INTERROGATION);
  equal(mmi.serviceCode, "00");
  equal(mmi.sia, "12");
  equal(mmi.sib, "34");
  equal(mmi.sic, "56");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_registration() {
  let mmi = parseMMI("**00*12*34*56#");

  equal(mmi.fullMMI, "**00*12*34*56#");
  equal(mmi.procedure, MMI_PROCEDURE_REGISTRATION);
  equal(mmi.serviceCode, "00");
  equal(mmi.sia, "12");
  equal(mmi.sib, "34");
  equal(mmi.sic, "56");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

add_test(function test_parseMMI_erasure() {
  let mmi = parseMMI("##00*12*34*56#");

  equal(mmi.fullMMI, "##00*12*34*56#");
  equal(mmi.procedure, MMI_PROCEDURE_ERASURE);
  equal(mmi.serviceCode, "00");
  equal(mmi.sia, "12");
  equal(mmi.sib, "34");
  equal(mmi.sic, "56");
  equal(mmi.pwd, undefined);
  equal(mmi.dialNumber, undefined);

  run_next_test();
});

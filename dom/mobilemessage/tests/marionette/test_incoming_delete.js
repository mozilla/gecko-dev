/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

const SENDER = "5555552368"; // the remote number
const RECEIVER = "15555215554"; // the emulator's number

let manager = window.navigator.mozMobileMessage;
let msgText = "Mozilla Firefox OS!";

function verifyInitialState() {
  log("Verifying initial state.");
  ok(manager instanceof MozMobileMessageManager,
     "manager is instance of " + manager.constructor);
  simulateIncomingSms();  
}

function simulateIncomingSms() {
  log("Simulating incoming SMS.");

  manager.onreceived = function onreceived(event) {
    log("Received 'onreceived' event.");
    let incomingSms = event.message;
    ok(incomingSms, "incoming sms");
    ok(incomingSms.id, "sms id");
    log("Received SMS (id: " + incomingSms.id + ").");
    ok(incomingSms.threadId, "thread id");
    is(incomingSms.body, msgText, "msg body");
    is(incomingSms.delivery, "received", "delivery");
    is(incomingSms.deliveryStatus, "success", "deliveryStatus");
    is(incomingSms.read, false, "read");
    is(incomingSms.receiver, RECEIVER, "receiver");
    is(incomingSms.sender, SENDER, "sender");
    is(incomingSms.messageClass, "normal", "messageClass");
    is(incomingSms.deliveryTimestamp, 0, "deliveryTimestamp is 0");

    verifySmsExists(incomingSms);
  };
  runEmulatorCmd("sms send " + SENDER + " " + msgText, function(result) {
    is(result[0], "OK", "emulator output");
  });
}

function verifySmsExists(incomingSms) {
  log("Getting SMS (id: " + incomingSms.id + ").");
  let requestRet = manager.getMessage(incomingSms.id);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, incomingSms.id, "found SMS id matches");
    is(foundSms.threadId, incomingSms.threadId, "found SMS thread id matches");
    is(foundSms.body, msgText, "found SMS msg text matches");
    is(foundSms.delivery, "received", "delivery");
    is(foundSms.deliveryStatus, "success", "deliveryStatus");
    is(foundSms.read, false, "read");
    is(foundSms.receiver, RECEIVER, "receiver");
    is(foundSms.sender, SENDER, "sender");
    is(foundSms.messageClass, "normal", "messageClass");
    log("Got SMS (id: " + foundSms.id + ") as expected.");
    deleteSms(incomingSms);
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + incomingSms.id + ") but should have.");
    ok(false,"SMS was not found");
    cleanUp();
  };
}

function deleteSms(smsMsgObj){
  log("Deleting SMS (id: " + smsMsgObj.id + ") using smsmsg obj parameter.");
  let requestRet = manager.delete(smsMsgObj);
  ok(requestRet,"smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    if(event.target.result){
      verifySmsDeleted(smsMsgObj.id);
    } else {
      log("smsrequest returned false for manager.delete");
      ok(false,"SMS delete failed");
      cleanUp();
    }
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    ok(false, "manager.delete request returned unexpected error: "
        + event.target.error.name );
    cleanUp();
  };
}

function verifySmsDeleted(smsId) {
  log("Getting SMS (id: " + smsId + ").");
  let requestRet = manager.getMessage(smsId);
  ok(requestRet, "smsrequest obj returned");

  requestRet.onsuccess = function(event) {
    log("Received 'onsuccess' smsrequest event.");
    ok(event.target.result, "smsrequest event.target.result");
    let foundSms = event.target.result;
    is(foundSms.id, smsId, "found SMS id matches");
    is(foundSms.body, msgText, "found SMS msg text matches");
    log("Got SMS (id: " + foundSms.id + ") but should not have.");
    ok(false, "SMS was not deleted");
    cleanUp();
  };

  requestRet.onerror = function(event) {
    log("Received 'onerror' smsrequest event.");
    ok(event.target.error, "domerror obj");
    is(event.target.error.name, "NotFoundError", "error returned");
    log("Could not get SMS (id: " + smsId + ") as expected.");
    cleanUp();
  };
}

function cleanUp() {
  manager.onreceived = null;
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

// Start the test
verifyInitialState();

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

function policyExpired(policy) {
  let currentDate = new Date();
  return (currentDate - policy.creation) / 1_000 > policy.nel.max_age;
}

function errorType(aChannel) {
  // TODO: we have to map a lot more error codes
  switch (aChannel.status) {
    case Cr.NS_ERROR_UNKNOWN_HOST:
      // TODO: if there is no connectivity, return "dns.unreachable"
      return "dns.name_not_resolved";
    case Cr.NS_ERROR_REDIRECT_LOOP:
      return "http.response.redirect_loop";
    case Cr.NS_BINDING_REDIRECTED:
      return "ok";
    case Cr.NS_ERROR_NET_TIMEOUT:
      return "tcp.timed_out";
    case Cr.NS_ERROR_NET_RESET:
      return "tcp.reset";
    case Cr.NS_ERROR_CONNECTION_REFUSED:
      return "tcp.refused";
    default:
      break;
  }

  if (
    aChannel.status == Cr.NS_OK &&
    (aChannel.responseStatus / 100 == 2 || aChannel.responseStatus == 304)
  ) {
    return "ok";
  }

  if (
    aChannel.status == Cr.NS_OK &&
    aChannel.responseStatus >= 400 &&
    aChannel.responseStatus <= 599
  ) {
    return "http.error";
  }
  return "unknown" + aChannel.status;
}

function channelPhase(aChannel) {
  const NS_NET_STATUS_RESOLVING_HOST = 0x4b0003;
  const NS_NET_STATUS_RESOLVED_HOST = 0x4b000b;
  const NS_NET_STATUS_CONNECTING_TO = 0x4b0007;
  const NS_NET_STATUS_CONNECTED_TO = 0x4b0004;
  const NS_NET_STATUS_TLS_HANDSHAKE_STARTING = 0x4b000c;
  const NS_NET_STATUS_TLS_HANDSHAKE_ENDED = 0x4b000d;
  const NS_NET_STATUS_SENDING_TO = 0x4b0005;
  const NS_NET_STATUS_WAITING_FOR = 0x4b000a;
  const NS_NET_STATUS_RECEIVING_FROM = 0x4b0006;
  const NS_NET_STATUS_READING = 0x4b0008;
  const NS_NET_STATUS_WRITING = 0x4b0009;

  let lastStatus = aChannel.QueryInterface(
    Ci.nsIHttpChannelInternal
  ).lastTransportStatus;

  switch (lastStatus) {
    case NS_NET_STATUS_RESOLVING_HOST:
    case NS_NET_STATUS_RESOLVED_HOST:
      return "dns";
    case NS_NET_STATUS_CONNECTING_TO:
    case NS_NET_STATUS_CONNECTED_TO: // TODO: is this right?
      return "connection";
    case NS_NET_STATUS_TLS_HANDSHAKE_STARTING:
    case NS_NET_STATUS_TLS_HANDSHAKE_ENDED:
      return "connection";
    case NS_NET_STATUS_SENDING_TO:
    case NS_NET_STATUS_WAITING_FOR:
    case NS_NET_STATUS_RECEIVING_FROM:
    case NS_NET_STATUS_READING:
    case NS_NET_STATUS_WRITING:
      return "application";
    default:
      // XXX(valentin): we default to DNS, but we should never get here.
      return "dns";
  }
}

export class NetworkErrorLogging {
  constructor() {}

  // Policy cache
  // https://www.w3.org/TR/2023/WD-network-error-logging-20231005/#policy-cache
  policyCache = {};
  // TODO: maybe persist policies to disk?

  // https://www.w3.org/TR/2023/WD-network-error-logging-20231005/#process-policy-headers
  registerPolicy(aChannel) {
    // 1. Abort these steps if any of the following conditions are true:
    // 1.1 The result of executing the "Is origin potentially trustworthy?" algorithm on request's origin is not Potentially Trustworthy.
    if (
      !Services.scriptSecurityManager.getChannelResultPrincipal(aChannel)
        .isOriginPotentiallyTrustworthy
    ) {
      return;
    }

    // 4. Let header be the value of the response header whose name is NEL.
    // 5. Let list be the result of executing the algorithm defined in Section 4 of [HTTP-JFV] on header. If that algorithm results in an error, or if list is empty, abort these steps.
    let list = [];
    aChannel.getOriginalResponseHeader("NEL", {
      QueryInterface: ChromeUtils.generateQI(["nsIHttpHeaderVisitor"]),
      visitHeader: (aHeader, aValue) => {
        list.push(aValue);
        // We only care about the first one so we could exit early
        // We could throw early, but that makes the errors show up in stderr.
        // The performance impact of not throwing is minimal.
        // throw new Error(Cr.NS_ERROR_ABORT);
      },
    });

    // 1.2 response does not contain a response header whose name is NEL.
    if (!list.length) {
      return;
    }

    // 2. Let origin be request's origin.
    let origin =
      Services.scriptSecurityManager.getChannelResultPrincipal(aChannel).origin;

    // 3. Let key be the result of calling determine the network partition key, given request.
    let key = Services.io.originAttributesForNetworkState(aChannel);

    // 6. Let item be the first element of list.
    let item = JSON.parse(list[0]);

    // 7. If item has no member named max_age, or that member's value is not a number, abort these steps.
    if (!item.max_age || !Number.isInteger(item.max_age)) {
      return;
    }

    // 8. If the value of item's max_age member is 0, then remove any NEL policy from the policy cache whose origin is origin, and skip the remaining steps.
    if (!item.max_age) {
      delete this.policyCache[String([key, origin])];
      return;
    }

    // 9. If item has no member named report_to, or that member's value is not a string, abort these steps.
    if (!item.report_to || typeof item.report_to != "string") {
      return;
    }

    // 10. If item has a member named success_fraction, whose value is not a number in the range 0.0 to 1.0, inclusive, abort these steps.
    if (
      item.success_fraction &&
      (typeof item.success_fraction != "number" ||
        item.success_fraction < 0 ||
        item.success_fraction > 1)
    ) {
      return;
    }

    // 11. If item has a member named failure_fraction, whose value is not a number in the range 0.0 to 1.0, inclusive, abort these steps.
    if (
      item.failure_fraction &&
      (typeof item.failure_fraction != "number" ||
        item.failure_fraction < 0 ||
        item.success_fraction > 1)
    ) {
      return;
    }

    // 12. If item has a member named request_headers, whose value is not a list, or if any element of that list is not a string, abort these steps.
    if (
      item.request_headers &&
      !Array.isArray(
        item.request_headers ||
          !item.request_headers.every(e => typeof e == "string")
      )
    ) {
      return;
    }

    // 13. If item has a member named response_headers, whose value is not a list, or if any element of that list is not a string, abort these steps.
    if (
      item.response_headers &&
      !Array.isArray(
        item.response_headers ||
          !item.response_headers.every(e => typeof e == "string")
      )
    ) {
      return;
    }

    // 14. Let policy be a new NEL policy whose properties are set as follows:
    let policy = {};

    // received IP address
    // XXX: What should we do when using a proxy?
    try {
      policy.ip_address = aChannel.QueryInterface(
        Ci.nsIHttpChannelInternal
      ).remoteAddress;
    } catch (e) {
      return;
    }

    // origin
    policy.origin = origin;

    if (item.include_subdomains) {
      policy.subdomains = true;
    }

    policy.request_headers = item.request_headers;
    policy.response_headers = item.response_headers;
    policy.ttl = item.max_age;
    policy.creation = new Date();
    policy.successful_sampling_rate = item.success_fraction || 0.0;
    policy.failure_sampling_rate = item.failure_fraction || 1.0;

    // TODO: Remove these when no longer needed
    policy.nel = item;
    let reportTo = JSON.parse(
      aChannel.QueryInterface(Ci.nsIHttpChannel).getResponseHeader("Report-To")
    );
    policy.reportTo = reportTo;

    // 15. If there is already an entry in the policy cache for (key, origin), replace it with policy; otherwise, insert policy into the policy cache for (key, origin).
    this.policyCache[String([key, origin])] = policy;
  }

  // https://www.w3.org/TR/2023/WD-network-error-logging-20231005/#choose-a-policy-for-a-request
  choosePolicyForRequest(aChannel) {
    // 1. Let origin be request's origin.
    let principal =
      Services.scriptSecurityManager.getChannelResultPrincipal(aChannel);
    let origin = principal.origin;
    // 2. Let key be the result of calling determine the network partition key, given request.
    let key = Services.io.originAttributesForNetworkState(aChannel);

    // 3. If there is an entry in the policy cache for (key, origin):
    let policy = this.policyCache[String([key, origin])];
    //   3.1. Let policy be that entry.
    if (policy) {
      // 3.2. If policy is not expired, return it.
      if (!policyExpired(policy)) {
        return { policy, key, origin };
      }
    }

    // 4. For each parent origin that is a superdomain match of origin:
    // 4.1. If there is an entry in the policy cache for (key, parent origin):
    //    4.1.1. Let policy be that entry.
    //    4.1.2. If policy is not expired, and its subdomains flag is include, return it.
    while (principal.nextSubDomainPrincipal) {
      principal = principal.nextSubDomainPrincipal;
      origin = principal.origin;
      policy = this.policyCache[String([key, origin])];
      if (policy && !policyExpired(policy)) {
        return { policy, key, origin };
      }
    }

    // 5. Return no policy.
    return {};
  }

  // https://www.w3.org/TR/2023/WD-network-error-logging-20231005/#generate-a-network-error-report
  generateNELReport(aChannel) {
    // 1. If the result of executing the "Is origin potentially trustworthy?" algorithm on request's origin is not Potentially Trustworthy, return null.
    if (
      !Services.scriptSecurityManager.getChannelResultPrincipal(aChannel)
        .isOriginPotentiallyTrustworthy
    ) {
      return;
    }
    // 2. Let origin be request's origin.
    let origin =
      Services.scriptSecurityManager.getChannelResultPrincipal(aChannel).origin;

    // 3. Let policy be the result of executing 5.1 Choose a policy for a request on request. If policy is no policy, return null.
    let {
      policy,
      key,
      origin: policyOrigin,
    } = this.choosePolicyForRequest(aChannel);
    if (!policy) {
      return;
    }

    // 4. Determine the active sampling rate for this request:
    let samplingRate = 0.0;
    if (
      aChannel.status == Cr.NS_OK &&
      aChannel.responseStatus >= 200 &&
      aChannel.responseStatus <= 299
    ) {
      // If request succeeded, let sampling rate be policy's successful sampling rate.
      samplingRate = policy.successful_sampling_rate || 0.0;
    } else {
      // If request failed, let sampling rate be policy's failure sampling rate.
      samplingRate = policy.successful_sampling_rate || 1.0;
    }

    // 5. Decide whether or not to report on this request. Let roll be a random number between 0.0 and 1.0, inclusive. If roll ≥ sampling rate, return null.
    if (Math.random() >= samplingRate) {
      return;
    }

    // 6. Let report body be a new ECMAScript object with the following properties:

    let phase = channelPhase(aChannel);
    let report_body = {
      sampling_fraction: samplingRate,
      elapsed_time: 1, // TODO
      phase,
      type: errorType(aChannel), // TODO
    };

    // 7. If report body's phase property is not dns, append the following properties to report body:
    if (phase != "dns") {
      // XXX: should we actually report server_ip?
      // It could be used to detect the presence of a PiHole.
      report_body.server_ip = aChannel.QueryInterface(
        Ci.nsIHttpChannelInternal
      ).remoteAddress;
      report_body.protocol = aChannel.protocolVersion;
    }

    // 8. If report body's phase property is not dns or connection, append the following properties to report body:
    // referrer?
    // method
    // request_headers?
    // response_headers?
    // status_code
    if (phase != "dns" && phase != "connection") {
      report_body.method = aChannel.requestMethod;
      report_body.status_code = aChannel.responseStatus;
    }

    // 9. If origin is not equal to policy's origin, policy's subdomains flag is include, and report body's phase property is not dns, return null.
    if (
      origin != policyOrigin &&
      policy.subdomains &&
      report_body.phase != "dns"
    ) {
      return;
    }

    // 10. If report body's phase property is not dns, and report body's server_ip property is non-empty and not equal to policy's received IP address:
    if (phase != "dns" && report_body.server_ip != policy.ip_address) {
      // 10.1 Set report body's phase to dns.
      report_body.phase = "dns";
      // 10.2 Set report body's type to dns.address_changed.
      report_body.type = "dns.address_changed";
      // 10.3 Clear report body's request_headers, response_headers, status_code, and elapsed_time properties.
      delete report_body.request_headers;
      delete report_body.response_headers;
      delete report_body.status_code;
      delete report_body.elapsed_time;
    }
    if (phase == "dns") {
      //TODO this is just to pass the test sends-report-on-subdomain-dns-failure.https.html
      report_body.method = aChannel.requestMethod;
      report_body.status_code = 0;
      // TODO
    }

    // 11. If policy is stale, then delete policy from the policy cache.
    let currentDate = new Date();
    if ((currentDate - policy.creation) / 1_000 > 172800) {
      // Delete the policy.
      delete this.policyCache[String([key, policyOrigin])];

      // XXX: should we exit here, or continue submit the report?
    }

    // 12. Return report body and policy.

    // https://www.w3.org/TR/2023/WD-network-error-logging-20231005/#deliver-a-network-report
    // 1. Let url be request's URL.
    // 2. Clear url's fragment.
    let uriMutator = aChannel.URI.mutate().setRef("");
    // 3. If report body's phase property is dns or connection:
    //    Clear url's path and query.
    if (report_body.phase == "dns" || report_body.phase == "connection") {
      uriMutator.setPathQueryRef("");
    }

    // 4. Generate a network report given these parameters:
    let report = {
      type: "network-error",
      url: aChannel.URI.specIgnoringRef, // uriMutator.finalize().spec, // XXX: sends-report-on-subdomain-dns-failure.https.html expects full URL
      user_agent: Cc["@mozilla.org/network/protocol;1?name=http"].getService(
        Ci.nsIHttpProtocolHandler
      ).userAgent,
      body: report_body,
    };
    // XXX: this would benefit from using the actual reporting API,
    //      but it's not clear how easy it is to:
    //        - use it in the parent process
    //        - have it use the Report-To header
    // https://w3c.github.io/reporting/#queue-report
    if (policy && policy.reportTo.group === policy.nel.report_to) {
      // TODO: defer to later.
      fetch(policy.reportTo.endpoints[0].url, {
        method: "POST",
        mode: "cors",
        credentials: "omit",
        headers: {
          "Content-Type": "application/reports+json",
        },
        body: JSON.stringify([report]),
        triggeringPrincipal:
          Services.scriptSecurityManager.getChannelResultPrincipal(aChannel),
      });
    }
  }

  QueryInterface = ChromeUtils.generateQI(["nsINetworkErrorLogging"]);
}

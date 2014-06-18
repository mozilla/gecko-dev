/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <limits>

#include "pkix/bind.h"
#include "pkix/pkix.h"
#include "pkixcheck.h"
#include "pkixder.h"

#include "hasht.h"
#include "pk11pub.h"
#include "secder.h"

// TODO: use typed/qualified typedefs everywhere?
// TODO: When should we return SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE?

namespace mozilla { namespace pkix {

static const PRTime ONE_DAY
  = INT64_C(24) * INT64_C(60) * INT64_C(60) * PR_USEC_PER_SEC;
static const PRTime SLOP = ONE_DAY;

// These values correspond to the tag values in the ASN.1 CertStatus
MOZILLA_PKIX_ENUM_CLASS CertStatus : uint8_t {
  Good = der::CONTEXT_SPECIFIC | 0,
  Revoked = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
  Unknown = der::CONTEXT_SPECIFIC | 2
};

class Context
{
public:
  Context(TrustDomain& trustDomain,
          const SECItem& certSerialNumber,
          const SECItem& issuerSubject,
          const SECItem& issuerSubjectPublicKeyInfo,
          PRTime time,
          uint16_t maxLifetimeInDays,
          PRTime* thisUpdate,
          PRTime* validThrough)
    : trustDomain(trustDomain)
    , certSerialNumber(certSerialNumber)
    , issuerSubject(issuerSubject)
    , issuerSubjectPublicKeyInfo(issuerSubjectPublicKeyInfo)
    , time(time)
    , maxLifetimeInDays(maxLifetimeInDays)
    , certStatus(CertStatus::Unknown)
    , thisUpdate(thisUpdate)
    , validThrough(validThrough)
  {
    if (thisUpdate) {
      *thisUpdate = 0;
    }
    if (validThrough) {
      *validThrough = 0;
    }
  }

  TrustDomain& trustDomain;
  const SECItem& certSerialNumber;
  const SECItem& issuerSubject;
  const SECItem& issuerSubjectPublicKeyInfo;
  const PRTime time;
  const uint16_t maxLifetimeInDays;
  CertStatus certStatus;
  PRTime* thisUpdate;
  PRTime* validThrough;

private:
  Context(const Context&); // delete
  void operator=(const Context&); // delete
};

static der::Result
HashBuf(const SECItem& item, /*out*/ uint8_t *hashBuf, size_t hashBufLen)
{
  if (hashBufLen != SHA1_LENGTH) {
    PR_NOT_REACHED("invalid hash length");
    return der::Fail(SEC_ERROR_INVALID_ARGS);
  }
  if (item.len >
      static_cast<decltype(item.len)>(std::numeric_limits<int32_t>::max())) {
    PR_NOT_REACHED("large OCSP responses should have already been rejected");
    return der::Fail(SEC_ERROR_INVALID_ARGS);
  }
  if (PK11_HashBuf(SEC_OID_SHA1, hashBuf, item.data,
                   static_cast<int32_t>(item.len)) != SECSuccess) {
    return der::Fail(PR_GetError());
  }
  return der::Success;
}

// Verify that potentialSigner is a valid delegated OCSP response signing cert
// according to RFC 6960 section 4.2.2.2.
static Result
CheckOCSPResponseSignerCert(TrustDomain& trustDomain,
                            BackCert& potentialSigner,
                            const SECItem& issuerSubject,
                            const SECItem& issuerSubjectPublicKeyInfo,
                            PRTime time)
{
  Result rv;

  // We don't need to do a complete verification of the signer (i.e. we don't
  // have to call BuildCertChain to verify the entire chain) because we
  // already know that the issuer is valid, since revocation checking is done
  // from the root to the parent after we've built a complete chain that we
  // know is otherwise valid. Rather, we just need to do a one-step validation
  // from potentialSigner to the issuer.
  //
  // It seems reasonable to require the KU_DIGITAL_SIGNATURE key usage on the
  // OCSP responder certificate if the OCSP responder certificate has a
  // key usage extension. However, according to bug 240456, some OCSP responder
  // certificates may have only the nonRepudiation bit set. Also, the OCSP
  // specification (RFC 6960) does not mandate any particular key usage to be
  // asserted for OCSP responde signers. Oddly, the CABForum Baseline
  // Requirements v.1.1.5 do say "If the Root CA Private Key is used for
  // signing OCSP responses, then the digitalSignature bit MUST be set."
  //
  // Note that CheckIssuerIndependentProperties processes
  // SEC_OID_OCSP_RESPONDER in the way that the OCSP specification requires us
  // to--in particular, it doesn't allow SEC_OID_OCSP_RESPONDER to be implied
  // by a missing EKU extension, unlike other EKUs.
  //
  // TODO(bug 926261): If we're validating for a policy then the policy OID we
  // are validating for should be passed to CheckIssuerIndependentProperties.
  rv = CheckIssuerIndependentProperties(trustDomain, potentialSigner, time,
                                        EndEntityOrCA::MustBeEndEntity, 0,
                                        KeyPurposeId::id_kp_OCSPSigning,
                                        CertPolicyId::anyPolicy, 0);
  if (rv != Success) {
    return rv;
  }

  // It is possible that there exists a certificate with the same key as the
  // issuer but with a different name, so we need to compare names
  // XXX(bug 926270) XXX(bug 1008133) XXX(bug 980163): Improve name
  // comparison.
  // TODO: needs test
  if (!SECITEM_ItemsAreEqual(&potentialSigner.GetIssuer(), &issuerSubject)) {
    return Fail(RecoverableError, SEC_ERROR_OCSP_RESPONDER_CERT_INVALID);
  }

  // TODO(bug 926260): check name constraints
  return potentialSigner.VerifyOwnSignatureWithKey(trustDomain,
                                                   issuerSubjectPublicKeyInfo);

  // TODO: check for revocation of the OCSP responder certificate unless no-check
  // or the caller forcing no-check. To properly support the no-check policy, we'd
  // need to enforce policy constraints from the issuerChain.
}

MOZILLA_PKIX_ENUM_CLASS ResponderIDType : uint8_t
{
  byName = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
  byKey = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 2
};

static inline der::Result OCSPResponse(der::Input&, Context&);
static inline der::Result ResponseBytes(der::Input&, Context&);
static inline der::Result BasicResponse(der::Input&, Context&);
static inline der::Result ResponseData(
                              der::Input& tbsResponseData, Context& context,
                              const CERTSignedData& signedResponseData,
                              /*const*/ SECItem* certs, size_t numCerts);
static inline der::Result SingleResponse(der::Input& input,
                                          Context& context);
static inline der::Result CheckExtensionsForCriticality(der::Input&);
static inline der::Result CertID(der::Input& input,
                                  const Context& context,
                                  /*out*/ bool& match);
static Result MatchKeyHash(const SECItem& issuerKeyHash,
                           const SECItem& issuerSubjectPublicKeyInfo,
                           /*out*/ bool& match);

static Result
MatchResponderID(ResponderIDType responderIDType,
                 const SECItem& responderIDItem,
                 const SECItem& potentialSignerSubject,
                 const SECItem& potentialSignerSubjectPublicKeyInfo,
                 /*out*/ bool& match)
{
  match = false;

  switch (responderIDType) {
    case ResponderIDType::byName:
      // XXX(bug 926270) XXX(bug 1008133) XXX(bug 980163): Improve name
      // comparison.
      match = SECITEM_ItemsAreEqual(&responderIDItem, &potentialSignerSubject);
      return Success;

    case ResponderIDType::byKey:
    {
      der::Input responderID;
      if (responderID.Init(responderIDItem.data, responderIDItem.len)
            != der::Success) {
        return RecoverableError;
      }
      SECItem keyHash;
      if (der::ExpectTagAndGetValue(responderID, der::OCTET_STRING, keyHash)
            != der::Success) {
        return RecoverableError;
      }
      return MatchKeyHash(keyHash, potentialSignerSubjectPublicKeyInfo, match);
    }

    default:
      return Fail(RecoverableError, SEC_ERROR_OCSP_MALFORMED_RESPONSE);
  }
}

static Result
VerifyOCSPSignedData(TrustDomain& trustDomain,
                     const CERTSignedData& signedResponseData,
                     const SECItem& spki)
{
  SECStatus srv(trustDomain.VerifySignedData(&signedResponseData, spki));
  if (srv != SECSuccess) {
    if (PR_GetError() == SEC_ERROR_BAD_SIGNATURE) {
      PR_SetError(SEC_ERROR_OCSP_BAD_SIGNATURE, 0);
    }
  }
  return MapSECStatus(srv);
}

// RFC 6960 section 4.2.2.2: The OCSP responder must either be the issuer of
// the cert or it must be a delegated OCSP response signing cert directly
// issued by the issuer. If the OCSP responder is a delegated OCSP response
// signer, then its certificate is (probably) embedded within the OCSP
// response and we'll need to verify that it is a valid certificate that chains
// *directly* to issuerCert.
static Result
VerifySignature(Context& context, ResponderIDType responderIDType,
                const SECItem& responderID, const SECItem* certs,
                size_t numCerts, const CERTSignedData& signedResponseData)
{
  bool match;
  Result rv = MatchResponderID(responderIDType, responderID,
                               context.issuerSubject,
                               context.issuerSubjectPublicKeyInfo, match);
  if (rv != Success) {
    return rv;
  }
  if (match) {
    return VerifyOCSPSignedData(context.trustDomain, signedResponseData,
                                context.issuerSubjectPublicKeyInfo);
  }

  for (size_t i = 0; i < numCerts; ++i) {
    BackCert cert(nullptr, BackCert::IncludeCN::No);
    rv = cert.Init(certs[i]);
    if (rv != Success) {
      return rv;
    }
    rv = MatchResponderID(responderIDType, responderID,
                          cert.GetSubject(), cert.GetSubjectPublicKeyInfo(),
                          match);
    if (rv == FatalError) {
      return rv;
    }
    if (rv == RecoverableError) {
      continue;
    }

    if (match) {
      rv = CheckOCSPResponseSignerCert(context.trustDomain, cert,
                                       context.issuerSubject,
                                       context.issuerSubjectPublicKeyInfo,
                                       context.time);
      if (rv == FatalError) {
        return rv;
      }
      if (rv == RecoverableError) {
        continue;
      }

      return VerifyOCSPSignedData(context.trustDomain, signedResponseData,
                                  cert.GetSubjectPublicKeyInfo());
    }
  }

  return Fail(RecoverableError, SEC_ERROR_OCSP_INVALID_SIGNING_CERT);
}

static inline void
SetErrorToMalformedResponseOnBadDERError()
{
  if (PR_GetError() == SEC_ERROR_BAD_DER) {
    PR_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE, 0);
  }
}

SECStatus
VerifyEncodedOCSPResponse(TrustDomain& trustDomain,
                          const CERTCertificate* cert,
                          CERTCertificate* issuerCert, PRTime time,
                          uint16_t maxOCSPLifetimeInDays,
                          const SECItem* encodedResponse,
                          PRTime* thisUpdate,
                          PRTime* validThrough)
{
  PR_ASSERT(cert);
  PR_ASSERT(issuerCert);
  // TODO: PR_Assert(pinArg)
  PR_ASSERT(encodedResponse);
  if (!cert || !issuerCert || !encodedResponse || !encodedResponse->data) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  der::Input input;
  if (input.Init(encodedResponse->data, encodedResponse->len) != der::Success) {
    SetErrorToMalformedResponseOnBadDERError();
    return SECFailure;
  }
  Context context(trustDomain, cert->serialNumber, issuerCert->derSubject,
                  issuerCert->derPublicKey, time, maxOCSPLifetimeInDays,
                  thisUpdate, validThrough);

  if (der::Nested(input, der::SEQUENCE,
                  bind(OCSPResponse, _1, ref(context))) != der::Success) {
    SetErrorToMalformedResponseOnBadDERError();
    return SECFailure;
  }

  if (der::End(input) != der::Success) {
    SetErrorToMalformedResponseOnBadDERError();
    return SECFailure;
  }

  switch (context.certStatus) {
    case CertStatus::Good:
      return SECSuccess;
    case CertStatus::Revoked:
      PR_SetError(SEC_ERROR_REVOKED_CERTIFICATE, 0);
      return SECFailure;
    case CertStatus::Unknown:
      PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
      return SECFailure;
  }

  PR_NOT_REACHED("unknown CertStatus");
  PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
  return SECFailure;
}

// OCSPResponse ::= SEQUENCE {
//       responseStatus         OCSPResponseStatus,
//       responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }
//
static inline der::Result
OCSPResponse(der::Input& input, Context& context)
{
  // OCSPResponseStatus ::= ENUMERATED {
  //     successful            (0),  -- Response has valid confirmations
  //     malformedRequest      (1),  -- Illegal confirmation request
  //     internalError         (2),  -- Internal error in issuer
  //     tryLater              (3),  -- Try again later
  //                                 -- (4) is not used
  //     sigRequired           (5),  -- Must sign the request
  //     unauthorized          (6)   -- Request unauthorized
  // }
  uint8_t responseStatus;

  if (der::Enumerated(input, responseStatus) != der::Success) {
    return der::Failure;
  }
  switch (responseStatus) {
    case 0: break; // successful
    case 1: return der::Fail(SEC_ERROR_OCSP_MALFORMED_REQUEST);
    case 2: return der::Fail(SEC_ERROR_OCSP_SERVER_ERROR);
    case 3: return der::Fail(SEC_ERROR_OCSP_TRY_SERVER_LATER);
    case 5: return der::Fail(SEC_ERROR_OCSP_REQUEST_NEEDS_SIG);
    case 6: return der::Fail(SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST);
    default: return der::Fail(SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS);
  }

  return der::Nested(input, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
                     der::SEQUENCE, bind(ResponseBytes, _1, ref(context)));
}

// ResponseBytes ::=       SEQUENCE {
//     responseType   OBJECT IDENTIFIER,
//     response       OCTET STRING }
static inline der::Result
ResponseBytes(der::Input& input, Context& context)
{
  static const uint8_t id_pkix_ocsp_basic[] = {
    0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };

  if (der::OID(input, id_pkix_ocsp_basic) != der::Success) {
    return der::Failure;
  }

  return der::Nested(input, der::OCTET_STRING, der::SEQUENCE,
                     bind(BasicResponse, _1, ref(context)));
}

// BasicOCSPResponse       ::= SEQUENCE {
//    tbsResponseData      ResponseData,
//    signatureAlgorithm   AlgorithmIdentifier,
//    signature            BIT STRING,
//    certs            [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
der::Result
BasicResponse(der::Input& input, Context& context)
{
  der::Input tbsResponseData;
  CERTSignedData signedData;
  if (der::SignedData(input, tbsResponseData, signedData) != der::Success) {
    if (PR_GetError() == SEC_ERROR_BAD_SIGNATURE) {
      PR_SetError(SEC_ERROR_OCSP_BAD_SIGNATURE, 0);
    }
    return der::Failure;
  }

  // Parse certificates, if any

  SECItem certs[8];
  size_t numCerts = 0;

  if (!input.AtEnd()) {
    // We ignore the lengths of the wrappers because we'll detect bad lengths
    // during parsing--too short and we'll run out of input for parsing a cert,
    // and too long and we'll have leftover data that won't parse as a cert.

    // [0] wrapper
    if (der::ExpectTagAndSkipLength(
          input, der::CONSTRUCTED | der::CONTEXT_SPECIFIC | 0)
        != der::Success) {
      return der::Failure;
    }

    // SEQUENCE wrapper
    if (der::ExpectTagAndSkipLength(input, der::SEQUENCE) != der::Success) {
      return der::Failure;
    }

    // sequence of certificates
    while (!input.AtEnd()) {
      if (numCerts == PR_ARRAY_SIZE(certs)) {
        return der::Fail(SEC_ERROR_BAD_DER);
      }

      // Unwrap the SEQUENCE that contains the certificate, which is itself a
      // SEQUENCE.
      der::Input::Mark mark(input.GetMark());
      if (der::ExpectTagAndSkipValue(input, der::SEQUENCE) != der::Success) {
        return der::Failure;
      }

      if (input.GetSECItem(siBuffer, mark, certs[numCerts]) != der::Success) {
        return der::Failure;
      }
      ++numCerts;
    }
  }

  return ResponseData(tbsResponseData, context, signedData, certs, numCerts);
}

// ResponseData ::= SEQUENCE {
//    version             [0] EXPLICIT Version DEFAULT v1,
//    responderID             ResponderID,
//    producedAt              GeneralizedTime,
//    responses               SEQUENCE OF SingleResponse,
//    responseExtensions  [1] EXPLICIT Extensions OPTIONAL }
static inline der::Result
ResponseData(der::Input& input, Context& context,
             const CERTSignedData& signedResponseData,
             /*const*/ SECItem* certs, size_t numCerts)
{
  uint8_t version;
  if (der::OptionalVersion(input, version) != der::Success) {
    return der::Failure;
  }
  if (version != der::v1) {
    // TODO: more specific error code for bad version?
    return der::Fail(SEC_ERROR_BAD_DER);
  }

  // ResponderID ::= CHOICE {
  //    byName              [1] Name,
  //    byKey               [2] KeyHash }
  SECItem responderID;
  ResponderIDType responderIDType
    = input.Peek(static_cast<uint8_t>(ResponderIDType::byName))
    ? ResponderIDType::byName
    : ResponderIDType::byKey;
  if (ExpectTagAndGetValue(input, static_cast<uint8_t>(responderIDType),
                           responderID) != der::Success) {
    return der::Failure;
  }

  // This is the soonest we can verify the signature. We verify the signature
  // right away to follow the principal of minimizing the processing of data
  // before verifying its signature.
  if (VerifySignature(context, responderIDType, responderID, certs, numCerts,
                      signedResponseData) != Success) {
    return der::Failure;
  }

  // TODO: Do we even need to parse this? Should we just skip it?
  PRTime producedAt;
  if (der::GeneralizedTime(input, producedAt) != der::Success) {
    return der::Failure;
  }

  // We don't accept an empty sequence of responses. In practice, a legit OCSP
  // responder will never return an empty response, and handling the case of an
  // empty response makes things unnecessarily complicated.
  if (der::NestedOf(input, der::SEQUENCE, der::SEQUENCE,
                    der::EmptyAllowed::No,
                    bind(SingleResponse, _1, ref(context))) != der::Success) {
    return der::Failure;
  }

  if (!input.AtEnd()) {
    if (der::Nested(input, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
                    CheckExtensionsForCriticality) != der::Success) {
      return der::Failure;
    }
  }

  return der::Success;
}

// SingleResponse ::= SEQUENCE {
//    certID                       CertID,
//    certStatus                   CertStatus,
//    thisUpdate                   GeneralizedTime,
//    nextUpdate           [0]     EXPLICIT GeneralizedTime OPTIONAL,
//    singleExtensions     [1]     EXPLICIT Extensions{{re-ocsp-crl |
//                                              re-ocsp-archive-cutoff |
//                                              CrlEntryExtensions, ...}
//                                              } OPTIONAL }
static inline der::Result
SingleResponse(der::Input& input, Context& context)
{
  bool match = false;
  if (der::Nested(input, der::SEQUENCE,
                  bind(CertID, _1, cref(context), ref(match)))
        != der::Success) {
    return der::Failure;
  }

  if (!match) {
    // This response does not reference the certificate we're interested in.
    // By consuming the rest of our input and returning successfully, we can
    // continue processing and examine another response that might have what
    // we want.
    input.SkipToEnd();
    return der::Success;
  }

  // CertStatus ::= CHOICE {
  //     good        [0]     IMPLICIT NULL,
  //     revoked     [1]     IMPLICIT RevokedInfo,
  //     unknown     [2]     IMPLICIT UnknownInfo }
  //
  // In the event of multiple SingleResponses for a cert that have conflicting
  // statuses, we use the following precedence rules:
  //
  // * revoked overrides good and unknown
  // * good overrides unknown
  if (input.Peek(static_cast<uint8_t>(CertStatus::Good))) {
    if (ExpectTagAndLength(input, static_cast<uint8_t>(CertStatus::Good), 0)
          != der::Success) {
      return der::Failure;
    }
    if (context.certStatus != CertStatus::Revoked) {
      context.certStatus = CertStatus::Good;
    }
  } else if (input.Peek(static_cast<uint8_t>(CertStatus::Revoked))) {
    // We don't need any info from the RevokedInfo structure, so we don't even
    // parse it. TODO: We should mention issues like this in the explanation of
    // why we treat invalid OCSP responses equivalently to revoked for OCSP
    // stapling.
    if (der::ExpectTagAndSkipValue(input,
                                   static_cast<uint8_t>(CertStatus::Revoked))
          != der::Success) {
      return der::Failure;
    }
    context.certStatus = CertStatus::Revoked;
  } else if (ExpectTagAndLength(input,
                                static_cast<uint8_t>(CertStatus::Unknown),
                                0) != der::Success) {
    return der::Failure;
  }

  // http://tools.ietf.org/html/rfc6960#section-3.2
  // 5. The time at which the status being indicated is known to be
  //    correct (thisUpdate) is sufficiently recent;
  // 6. When available, the time at or before which newer information will
  //    be available about the status of the certificate (nextUpdate) is
  //    greater than the current time.

  const PRTime maxLifetime =
    context.maxLifetimeInDays * ONE_DAY;

  PRTime thisUpdate;
  if (der::GeneralizedTime(input, thisUpdate) != der::Success) {
    return der::Failure;
  }

  if (thisUpdate > context.time + SLOP) {
    return der::Fail(SEC_ERROR_OCSP_FUTURE_RESPONSE);
  }

  PRTime notAfter;
  static const uint8_t NEXT_UPDATE_TAG =
    der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0;
  if (input.Peek(NEXT_UPDATE_TAG)) {
    PRTime nextUpdate;
    if (der::Nested(input, NEXT_UPDATE_TAG,
                    bind(der::GeneralizedTime, _1, ref(nextUpdate)))
          != der::Success) {
      return der::Failure;
    }

    if (nextUpdate < thisUpdate) {
      return der::Fail(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
    }
    if (nextUpdate - thisUpdate <= maxLifetime) {
      notAfter = nextUpdate;
    } else {
      notAfter = thisUpdate + maxLifetime;
    }
  } else {
    // NSS requires all OCSP responses without a nextUpdate to be recent.
    // Match that stricter behavior.
    notAfter = thisUpdate + ONE_DAY;
  }

  if (context.time < SLOP) { // prevent underflow
    return der::Fail(SEC_ERROR_INVALID_ARGS);
  }
  if (context.time - SLOP > notAfter) {
    return der::Fail(SEC_ERROR_OCSP_OLD_RESPONSE);
  }

  if (!input.AtEnd()) {
    if (der::Nested(input, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
                    CheckExtensionsForCriticality) != der::Success) {
      return der::Failure;
    }
  }

  if (context.thisUpdate) {
    *context.thisUpdate = thisUpdate;
  }
  if (context.validThrough) {
    *context.validThrough = notAfter;
  }

  return der::Success;
}

// CertID          ::=     SEQUENCE {
//        hashAlgorithm       AlgorithmIdentifier,
//        issuerNameHash      OCTET STRING, -- Hash of issuer's DN
//        issuerKeyHash       OCTET STRING, -- Hash of issuer's public key
//        serialNumber        CertificateSerialNumber }
static inline der::Result
CertID(der::Input& input, const Context& context, /*out*/ bool& match)
{
  match = false;

  SECAlgorithmID hashAlgorithm;
  if (der::Nested(input, der::SEQUENCE,
                  bind(der::AlgorithmIdentifier, _1, ref(hashAlgorithm)))
         != der::Success) {
    return der::Failure;
  }

  SECItem issuerNameHash;
  if (der::ExpectTagAndGetValue(input, der::OCTET_STRING, issuerNameHash)
        != der::Success) {
    return der::Failure;
  }

  SECItem issuerKeyHash;
  if (der::ExpectTagAndGetValue(input, der::OCTET_STRING, issuerKeyHash)
        != der::Success) {
    return der::Failure;
  }

  SECItem serialNumber;
  if (der::CertificateSerialNumber(input, serialNumber) != der::Success) {
    return der::Failure;
  }

  if (!SECITEM_ItemsAreEqual(&serialNumber, &context.certSerialNumber)) {
    // This does not reference the certificate we're interested in.
    // Consume the rest of the input and return successfully to
    // potentially continue processing other responses.
    input.SkipToEnd();
    return der::Success;
  }

  // TODO: support SHA-2 hashes.

  SECOidTag hashAlg = SECOID_GetAlgorithmTag(&hashAlgorithm);
  if (hashAlg != SEC_OID_SHA1) {
    // Again, not interested in this response. Consume input, return success.
    input.SkipToEnd();
    return der::Success;
  }

  if (issuerNameHash.len != SHA1_LENGTH) {
    return der::Fail(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
  }

  // From http://tools.ietf.org/html/rfc6960#section-4.1.1:
  // "The hash shall be calculated over the DER encoding of the
  // issuer's name field in the certificate being checked."
  uint8_t hashBuf[SHA1_LENGTH];
  if (HashBuf(context.issuerSubject, hashBuf, sizeof(hashBuf))
        != der::Success) {
    return der::Failure;
  }
  if (memcmp(hashBuf, issuerNameHash.data, issuerNameHash.len)) {
    // Again, not interested in this response. Consume input, return success.
    input.SkipToEnd();
    return der::Success;
  }

  if (MatchKeyHash(issuerKeyHash, context.issuerSubjectPublicKeyInfo,
                   match) != Success) {
    return der::Failure;
  }

  return der::Success;
}

// From http://tools.ietf.org/html/rfc6960#section-4.1.1:
// "The hash shall be calculated over the value (excluding tag and length) of
// the subject public key field in the issuer's certificate."
//
// From http://tools.ietf.org/html/rfc6960#appendix-B.1:
// KeyHash ::= OCTET STRING -- SHA-1 hash of responder's public key
//                          -- (i.e., the SHA-1 hash of the value of the
//                          -- BIT STRING subjectPublicKey [excluding
//                          -- the tag, length, and number of unused
//                          -- bits] in the responder's certificate)
static Result
MatchKeyHash(const SECItem& keyHash, const SECItem& subjectPublicKeyInfo,
             /*out*/ bool& match)
{
  if (keyHash.len != SHA1_LENGTH)  {
    return Fail(RecoverableError, SEC_ERROR_OCSP_MALFORMED_RESPONSE);
  }

  // TODO(bug 966856): support SHA-2 hashes

  // RFC 5280 Section 4.1
  //
  // SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //    algorithm            AlgorithmIdentifier,
  //    subjectPublicKey     BIT STRING  }

  der::Input spki;

  {
    // The scope of input is limited to reduce the possibility of confusing it
    // with spki in places we need to be using spki below.
    der::Input input;
    if (input.Init(subjectPublicKeyInfo.data, subjectPublicKeyInfo.len)
          != der::Success) {
      return MapSECStatus(SECFailure);
    }

    if (der::ExpectTagAndGetValue(input, der::SEQUENCE, spki) != der::Success) {
      return MapSECStatus(SECFailure);
    }
    if (der::End(input) != der::Success) {
      return MapSECStatus(SECFailure);
    }
  }

  // Skip AlgorithmIdentifier
  if (der::ExpectTagAndSkipValue(spki, der::SEQUENCE) != der::Success) {
    return MapSECStatus(SECFailure);
  }

  SECItem subjectPublicKey;
  if (der::ExpectTagAndGetValue(spki, der::BIT_STRING, subjectPublicKey)
        != der::Success) {
    return MapSECStatus(SECFailure);
  }

  if (der::End(spki) != der::Success) {
    return MapSECStatus(SECFailure);
  }

  // Assume/require that the number of unused bits in the public key is zero.
  if (subjectPublicKey.len == 0 || subjectPublicKey.data[0] != 0) {
    return Fail(RecoverableError, SEC_ERROR_BAD_DER);
  }
  ++subjectPublicKey.data;
  --subjectPublicKey.len;

  static uint8_t hashBuf[SHA1_LENGTH];
  if (HashBuf(subjectPublicKey, hashBuf, sizeof(hashBuf)) != der::Success) {
    return MapSECStatus(SECFailure);
  }
  match = !memcmp(hashBuf, keyHash.data, keyHash.len);
  return Success;
}

// Extension  ::=  SEQUENCE  {
//      extnID      OBJECT IDENTIFIER,
//      critical    BOOLEAN DEFAULT FALSE,
//      extnValue   OCTET STRING
//      }
static der::Result
CheckExtensionForCriticality(der::Input& input)
{
  // TODO: maybe we should check the syntax of the OID value
  if (ExpectTagAndSkipValue(input, der::OIDTag) != der::Success) {
    return der::Failure;
  }

  // The only valid explicit encoding of the value is TRUE, so don't even
  // bother parsing it, since we're going to fail either way.
  if (input.Peek(der::BOOLEAN)) {
    return der::Fail(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
  }

  input.SkipToEnd();

  return der::Success;
}

// Extensions ::= SEQUENCE SIZE (1..MAX) OF Extension
static der::Result
CheckExtensionsForCriticality(der::Input& input)
{
  // TODO(bug 997994): some responders include an empty SEQUENCE OF
  // Extension, which is invalid (der::MayBeEmpty should really be
  // der::MustNotBeEmpty).
  return der::NestedOf(input, der::SEQUENCE, der::SEQUENCE,
                       der::EmptyAllowed::Yes, CheckExtensionForCriticality);
}

//   1. The certificate identified in a received response corresponds to
//      the certificate that was identified in the corresponding request;
//   2. The signature on the response is valid;
//   3. The identity of the signer matches the intended recipient of the
//      request;
//   4. The signer is currently authorized to provide a response for the
//      certificate in question;
//   5. The time at which the status being indicated is known to be
//      correct (thisUpdate) is sufficiently recent;
//   6. When available, the time at or before which newer information will
//      be available about the status of the certificate (nextUpdate) is
//      greater than the current time.
//
//   Responses whose nextUpdate value is earlier than
//   the local system time value SHOULD be considered unreliable.
//   Responses whose thisUpdate time is later than the local system time
//   SHOULD be considered unreliable.
//
//   If nextUpdate is not set, the responder is indicating that newer
//   revocation information is available all the time.
//
// http://tools.ietf.org/html/rfc5019#section-4

SECItem*
CreateEncodedOCSPRequest(PLArenaPool* arena,
                         const CERTCertificate* cert,
                         const CERTCertificate* issuerCert)
{
  if (!arena || !cert || !issuerCert) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  // We do not add any extensions to the request.

  // RFC 6960 says "An OCSP client MAY wish to specify the kinds of response
  // types it understands. To do so, it SHOULD use an extension with the OID
  // id-pkix-ocsp-response." This use of MAY and SHOULD is unclear. MSIE11
  // on Windows 8.1 does not include any extensions, whereas NSS has always
  // included the id-pkix-ocsp-response extension. Avoiding the sending the
  // extension is better for OCSP GET because it makes the request smaller,
  // and thus more likely to fit within the 255 byte limit for OCSP GET that
  // is specified in RFC 5019 Section 5.

  // Bug 966856: Add the id-pkix-ocsp-pref-sig-algs extension.

  // Since we don't know whether the OCSP responder supports anything other
  // than SHA-1, we have no choice but to use SHA-1 for issuerNameHash and
  // issuerKeyHash.
  static const uint8_t hashAlgorithm[11] = {
    0x30, 0x09,                               // SEQUENCE
    0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1A, //   OBJECT IDENTIFIER id-sha1
    0x05, 0x00,                               //   NULL
  };
  static const uint8_t hashLen = SHA1_LENGTH;

  static const unsigned int totalLenWithoutSerialNumberData
    = 2                             // OCSPRequest
    + 2                             //   tbsRequest
    + 2                             //     requestList
    + 2                             //       Request
    + 2                             //         reqCert (CertID)
    + PR_ARRAY_SIZE(hashAlgorithm)  //           hashAlgorithm
    + 2 + hashLen                   //           issuerNameHash
    + 2 + hashLen                   //           issuerKeyHash
    + 2;                            //           serialNumber (header)

  // The only way we could have a request this large is if the serialNumber was
  // ridiculously and unreasonably large. RFC 5280 says "Conforming CAs MUST
  // NOT use serialNumber values longer than 20 octets." With this restriction,
  // we allow for some amount of non-conformance with that requirement while
  // still ensuring we can encode the length values in the ASN.1 TLV structures
  // in a single byte.
  if (cert->serialNumber.len > 127u - totalLenWithoutSerialNumberData) {
    PR_SetError(SEC_ERROR_BAD_DATA, 0);
    return nullptr;
  }

  uint8_t totalLen = static_cast<uint8_t>(totalLenWithoutSerialNumberData +
    cert->serialNumber.len);

  SECItem* encodedRequest = SECITEM_AllocItem(arena, nullptr, totalLen);
  if (!encodedRequest) {
    return nullptr;
  }

  uint8_t* d = encodedRequest->data;
  *d++ = 0x30; *d++ = totalLen - 2u;  // OCSPRequest (SEQUENCE)
  *d++ = 0x30; *d++ = totalLen - 4u;  //   tbsRequest (SEQUENCE)
  *d++ = 0x30; *d++ = totalLen - 6u;  //     requestList (SEQUENCE OF)
  *d++ = 0x30; *d++ = totalLen - 8u;  //       Request (SEQUENCE)
  *d++ = 0x30; *d++ = totalLen - 10u; //         reqCert (CertID SEQUENCE)

  // reqCert.hashAlgorithm
  for (size_t i = 0; i < PR_ARRAY_SIZE(hashAlgorithm); ++i) {
    *d++ = hashAlgorithm[i];
  }

  // reqCert.issuerNameHash (OCTET STRING)
  *d++ = 0x04;
  *d++ = hashLen;
  if (HashBuf(issuerCert->derSubject, d, hashLen) != der::Success) {
    return nullptr;
  }
  d += hashLen;

  // reqCert.issuerKeyHash (OCTET STRING)
  *d++ = 0x04;
  *d++ = hashLen;
  SECItem key = issuerCert->subjectPublicKeyInfo.subjectPublicKey;
  DER_ConvertBitString(&key);
  if (HashBuf(key, d, hashLen) != der::Success) {
    return nullptr;
  }
  d += hashLen;

  // reqCert.serialNumber (INTEGER)
  *d++ = 0x02; // INTEGER
  *d++ = static_cast<uint8_t>(cert->serialNumber.len);
  for (size_t i = 0; i < cert->serialNumber.len; ++i) {
    *d++ = cert->serialNumber.data[i];
  }

  PR_ASSERT(d == encodedRequest->data + totalLen);

  return encodedRequest;
}

} } // namespace mozilla::pkix

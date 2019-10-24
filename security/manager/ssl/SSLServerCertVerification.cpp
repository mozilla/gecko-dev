/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// For connections that are not processed on the socket transport thread, we do
// NOT use the async logic described below. Instead, we authenticate the
// certificate on the thread that the connection's I/O happens on,
// synchronously. This allows us to do certificate verification for blocking
// (not non-blocking) sockets and sockets that have their I/O processed on a
// thread other than the socket transport service thread. Also, we DO NOT
// support blocking sockets on the socket transport service thread at all.
//
// During certificate authentication, we call CERT_PKIXVerifyCert or
// CERT_VerifyCert. These functions may make zero or more HTTP requests
// for OCSP responses, CRLs, intermediate certificates, etc. Our fetching logic
// for these requests processes them on the socket transport service thread.
//
// If the connection for which we are verifying the certificate is happening
// on the socket transport thread (the usually case, at least for HTTP), then
// if our cert auth hook were to call the CERT_*Verify* functions directly,
// there would be a deadlock: The CERT_*Verify* function would cause an event
// to be asynchronously posted to the socket transport thread, and then it
// would block the socket transport thread waiting to be notified of the HTTP
// response. However, the HTTP request would never actually be processed
// because the socket transport thread would be blocked and so it wouldn't be
// able process HTTP requests. (i.e. Deadlock.)
//
// Consequently, when we are asked to verify a certificate on the socket
// transport service thread, we must always call the CERT_*Verify* cert
// functions on another thread. To accomplish this, our auth cert hook
// dispatches a SSLServerCertVerificationJob to a pool of background threads,
// and then immediately returns SECWouldBlock to libssl. These jobs are where
// the CERT_*Verify* functions are actually called.
//
// When our auth cert hook returns SECWouldBlock, libssl will carry on the
// handshake while we validate the certificate. This will free up the socket
// transport thread so that HTTP requests--in particular, the OCSP/CRL/cert
// requests needed for cert verification as mentioned above--can be processed.
//
// Once the CERT_*Verify* function returns, the cert verification job
// dispatches a SSLServerCertVerificationResult to the socket transport thread;
// the SSLServerCertVerificationResult will notify libssl that the certificate
// authentication is complete. Once libssl is notified that the authentication
// is complete, it will continue the SSL handshake (if it hasn't already
// finished) and it will begin allowing us to send/receive data on the
// connection.
//
// Timeline of events (for connections managed by the socket transport service):
//
//    * libssl calls SSLServerCertVerificationJob::Dispatch on the socket
//      transport thread.
//    * SSLServerCertVerificationJob::Dispatch queues a job
//      (instance of SSLServerCertVerificationJob) to its background thread
//      pool and returns.
//    * One of the background threads calls CERT_*Verify*, which may enqueue
//      some HTTP request(s) onto the socket transport thread, and then
//      blocks that background thread waiting for the responses and/or timeouts
//      or errors for those requests.
//    * Once those HTTP responses have all come back or failed, the
//      CERT_*Verify* function returns a result indicating that the validation
//      succeeded or failed.
//    * If the validation succeeded, then a SSLServerCertVerificationResult
//      event is posted to the socket transport thread, and the cert
//      verification thread becomes free to verify other certificates.
//    * Otherwise, we do cert override processing to see if the validation
//      error can be convered by override rules. If yes, a
//      SSLServerCertVerificationResult event is posted to notify the successful
//      result of the override processing. If not, a NotifyCertProblemRunnable
//      is posted to the socket transport thread and then to the main thread
//      (blocking both, see NotifyCertProblemRunnable) to do bad cert
//      listener notification. Then it returns, freeing up the main thread.
//    * At the end of SSLServerCertVerificationJob::Run, we dispatch a
//      SSLServerCertVerificationResult event to the socket transport
//      thread to notify it of the error result of the override processing.
//    * The SSLServerCertVerificationResult event will either wake up the
//      socket (using SSL_RestartHandshakeAfterServerCert) if validation
//      succeeded or there was an error override, or it will set an error flag
//      so that the next I/O operation on the socket will fail, causing the
//      socket transport thread to close the connection.
//
// SSLServerCertVerificationResult must be dispatched to the socket transport
// thread because we must only call SSL_* functions on the socket transport
// thread since they may do I/O, because many parts of nsNSSSocketInfo (the
// subclass of TransportSecurityInfo used when validating certificates during
// an SSL handshake) and the PSM NSS I/O layer are not thread-safe, and because
// we need the event to interrupt the PR_Poll that may waiting for I/O on the
// socket for which we are validating the cert.

#include "SSLServerCertVerification.h"

#include <cstring>

#include "BRNameMatchingPolicy.h"
#include "CertVerifier.h"
#include "CryptoTask.h"
#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "PSMRunnable.h"
#include "RootCertificateTelemetryUtils.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "SharedSSLState.h"
#include "TransportSecurityInfo.h"  // For RememberCertErrorsTable
#include "cert.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/net/DNS.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIBadCertListener2.h"
#include "nsICertOverrideService.h"
#include "nsISiteSecurityService.h"
#include "nsISocketProvider.h"
#include "nsThreadPool.h"
#include "nsNetUtil.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsURLHelper.h"
#include "nsXPCOMCIDInternal.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixnss.h"
#include "secerr.h"
#include "secoidt.h"
#include "secport.h"
#include "ssl.h"
#include "sslerr.h"

extern mozilla::LazyLogModule gPIPNSSLog;

using namespace mozilla::pkix;

namespace mozilla {
namespace psm {

namespace {

// do not use a nsCOMPtr to avoid static initializer/destructor
nsIThreadPool* gCertVerificationThreadPool = nullptr;

}  // unnamed namespace

// Called when the socket transport thread starts, to initialize the SSL cert
// verification thread pool. By tying the thread pool startup/shutdown directly
// to the STS thread's lifetime, we ensure that they are *always* available for
// SSL connections and that there are no races during startup and especially
// shutdown. (Previously, we have had multiple problems with races in PSM
// background threads, and the race-prevention/shutdown logic used there is
// brittle. Since this service is critical to things like downloading updates,
// we take no chances.) Also, by doing things this way, we avoid the need for
// locks, since gCertVerificationThreadPool is only ever accessed on the socket
// transport thread.
void InitializeSSLServerCertVerificationThreads() {
  // TODO: tuning, make parameters preferences
  gCertVerificationThreadPool = new nsThreadPool();
  NS_ADDREF(gCertVerificationThreadPool);

  (void)gCertVerificationThreadPool->SetIdleThreadLimit(5);
  (void)gCertVerificationThreadPool->SetIdleThreadTimeout(30 * 1000);
  (void)gCertVerificationThreadPool->SetThreadLimit(5);
  (void)gCertVerificationThreadPool->SetName(NS_LITERAL_CSTRING("SSL Cert"));
}

// Called when the socket transport thread finishes, to destroy the thread
// pool. Since the socket transport service has stopped processing events, it
// will not attempt any more SSL I/O operations, so it is clearly safe to shut
// down the SSL cert verification infrastructure. Also, the STS will not
// dispatch many SSL verification result events at this point, so any pending
// cert verifications will (correctly) fail at the point they are dispatched.
//
// The other shutdown race condition that is possible is a race condition with
// shutdown of the nsNSSComponent service. We use the
// nsNSSShutdownPreventionLock where needed (not here) to prevent that.
void StopSSLServerCertVerificationThreads() {
  if (gCertVerificationThreadPool) {
    gCertVerificationThreadPool->Shutdown();
    NS_RELEASE(gCertVerificationThreadPool);
  }
}

namespace {

// Dispatched to the STS thread to notify the infoObject of the verification
// result.
//
// This will cause the PR_Poll in the STS thread to return, so things work
// correctly even if the STS thread is blocked polling (only) on the file
// descriptor that is waiting for this result.
class SSLServerCertVerificationResult : public Runnable {
 public:
  NS_DECL_NSIRUNNABLE

  SSLServerCertVerificationResult(TransportSecurityInfo* infoObject,
                                  PRErrorCode errorCode);

  void Dispatch();

 private:
  const RefPtr<TransportSecurityInfo> mInfoObject;
  const PRErrorCode mErrorCode;
};

class NotifyCertProblemRunnable : public SyncRunnableBase {
 public:
  NotifyCertProblemRunnable(uint64_t fdForLogging,
                            TransportSecurityInfo* infoObject)
      : mFdForLogging(fdForLogging), mInfoObject(infoObject) {}

  virtual void RunOnTargetThread() override;

 private:
  uint64_t mFdForLogging;
  const RefPtr<TransportSecurityInfo> mInfoObject;
};

// A probe value of 1 means "no error".
uint32_t MapOverridableErrorToProbeValue(PRErrorCode errorCode) {
  switch (errorCode) {
    case SEC_ERROR_UNKNOWN_ISSUER:
      return 2;
    case SEC_ERROR_CA_CERT_INVALID:
      return 3;
    case SEC_ERROR_UNTRUSTED_ISSUER:
      return 4;
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
      return 5;
    case SEC_ERROR_UNTRUSTED_CERT:
      return 6;
    case SEC_ERROR_INADEQUATE_KEY_USAGE:
      return 7;
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
      return 8;
    case SSL_ERROR_BAD_CERT_DOMAIN:
      return 9;
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return 10;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY:
      return 11;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA:
      return 12;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE:
      return 13;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE:
      return 14;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE:
      return 15;
    case SEC_ERROR_INVALID_TIME:
      return 16;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME:
      return 17;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED:
      return 18;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT:
      return 19;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_MITM_DETECTED:
      return 20;
  }
  NS_WARNING(
      "Unknown certificate error code. Does MapOverridableErrorToProbeValue "
      "handle everything in DetermineCertOverrideErrors?");
  return 0;
}

static uint32_t MapCertErrorToProbeValue(PRErrorCode errorCode) {
  uint32_t probeValue;
  switch (errorCode) {
    // see security/pkix/include/pkix/Result.h
#define MOZILLA_PKIX_MAP(name, value, nss_name) \
  case nss_name:                                \
    probeValue = value;                         \
    break;
    MOZILLA_PKIX_MAP_LIST
#undef MOZILLA_PKIX_MAP
    default:
      return 0;
  }

  // Since FATAL_ERROR_FLAG is 0x800, fatal error values are much larger than
  // non-fatal error values. To conserve space, we remap these so they start at
  // (decimal) 90 instead of 0x800. Currently there are ~50 non-fatal errors
  // mozilla::pkix might return, so saving space for 90 should be sufficient
  // (similarly, there are 4 fatal errors, so saving space for 10 should also
  // be sufficient).
  static_assert(
      FATAL_ERROR_FLAG == 0x800,
      "mozilla::pkix::FATAL_ERROR_FLAG is not what we were expecting");
  if (probeValue & FATAL_ERROR_FLAG) {
    probeValue ^= FATAL_ERROR_FLAG;
    probeValue += 90;
  }
  return probeValue;
}

SECStatus DetermineCertOverrideErrors(const UniqueCERTCertificate& cert,
                                      const nsACString& hostName, PRTime now,
                                      PRErrorCode defaultErrorCodeToReport,
                                      /*out*/ uint32_t& collectedErrors,
                                      /*out*/ PRErrorCode& errorCodeTrust,
                                      /*out*/ PRErrorCode& errorCodeMismatch,
                                      /*out*/ PRErrorCode& errorCodeTime) {
  MOZ_ASSERT(cert);
  MOZ_ASSERT(collectedErrors == 0);
  MOZ_ASSERT(errorCodeTrust == 0);
  MOZ_ASSERT(errorCodeMismatch == 0);
  MOZ_ASSERT(errorCodeTime == 0);

  // Assumes the error prioritization described in mozilla::pkix's
  // BuildForward function. Also assumes that CheckCertHostname was only
  // called if CertVerifier::VerifyCert succeeded.
  switch (defaultErrorCodeToReport) {
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_MITM_DETECTED:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA: {
      collectedErrors = nsICertOverrideService::ERROR_UNTRUSTED;
      errorCodeTrust = defaultErrorCodeToReport;

      SECCertTimeValidity validity =
          CERT_CheckCertValidTimes(cert.get(), now, false);
      if (validity == secCertTimeUndetermined) {
        // This only happens if cert is null. CERT_CheckCertValidTimes will
        // have set the error code to SEC_ERROR_INVALID_ARGS. We should really
        // be using mozilla::pkix here anyway.
        MOZ_ASSERT(PR_GetError() == SEC_ERROR_INVALID_ARGS);
        return SECFailure;
      }
      if (validity == secCertTimeExpired) {
        collectedErrors |= nsICertOverrideService::ERROR_TIME;
        errorCodeTime = SEC_ERROR_EXPIRED_CERTIFICATE;
      } else if (validity == secCertTimeNotValidYet) {
        collectedErrors |= nsICertOverrideService::ERROR_TIME;
        errorCodeTime =
            mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE;
      }
      break;
    }

    case SEC_ERROR_INVALID_TIME:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE:
      collectedErrors = nsICertOverrideService::ERROR_TIME;
      errorCodeTime = defaultErrorCodeToReport;
      break;

    case SSL_ERROR_BAD_CERT_DOMAIN:
      collectedErrors = nsICertOverrideService::ERROR_MISMATCH;
      errorCodeMismatch = SSL_ERROR_BAD_CERT_DOMAIN;
      break;

    case 0:
      NS_ERROR("No error code set during certificate validation failure.");
      PR_SetError(PR_INVALID_STATE_ERROR, 0);
      return SECFailure;

    default:
      PR_SetError(defaultErrorCodeToReport, 0);
      return SECFailure;
  }

  if (defaultErrorCodeToReport != SSL_ERROR_BAD_CERT_DOMAIN) {
    Input certInput;
    if (certInput.Init(cert->derCert.data, cert->derCert.len) != Success) {
      PR_SetError(SEC_ERROR_BAD_DER, 0);
      return SECFailure;
    }
    Input hostnameInput;
    Result result = hostnameInput.Init(
        BitwiseCast<const uint8_t*, const char*>(hostName.BeginReading()),
        hostName.Length());
    if (result != Success) {
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
    }
    // Use a lax policy so as to not generate potentially spurious name
    // mismatch "hints".
    BRNameMatchingPolicy nameMatchingPolicy(
        BRNameMatchingPolicy::Mode::DoNotEnforce);
    // CheckCertHostname expects that its input represents a certificate that
    // has already been successfully validated by BuildCertChain. This is
    // obviously not the case, however, because we're in the error path of
    // certificate verification. Thus, this is problematic. In the future, it
    // would be nice to remove this optimistic additional error checking and
    // simply punt to the front-end, which can more easily (and safely) perform
    // extra checks to give the user hints as to why verification failed.
    result = CheckCertHostname(certInput, hostnameInput, nameMatchingPolicy);
    // Treat malformed name information as a domain mismatch.
    if (result == Result::ERROR_BAD_DER ||
        result == Result::ERROR_BAD_CERT_DOMAIN) {
      collectedErrors |= nsICertOverrideService::ERROR_MISMATCH;
      errorCodeMismatch = SSL_ERROR_BAD_CERT_DOMAIN;
    } else if (IsFatalError(result)) {
      // Because its input has not been validated by BuildCertChain,
      // CheckCertHostname can return an error that is less important than the
      // original certificate verification error. Only return an error result
      // from this function if we've encountered a fatal error.
      PR_SetError(MapResultToPRErrorCode(result), 0);
      return SECFailure;
    }
  }

  return SECSuccess;
}

// Helper function to determine if overrides are allowed for this host.
// Overrides are not allowed for known HSTS or HPKP hosts. However, an IP
// address is never considered an HSTS or HPKP host.
static nsresult OverrideAllowedForHost(
    uint64_t aPtrForLog, const nsACString& aHostname,
    const OriginAttributes& aOriginAttributes, uint32_t aProviderFlags,
    /*out*/ bool& aOverrideAllowed) {
  aOverrideAllowed = false;

  // If this is an IP address, overrides are allowed, because an IP address is
  // never an HSTS or HPKP host. nsISiteSecurityService takes this into account
  // already, but the real problem here is that calling NS_NewURI with an IPv6
  // address fails. We do this to avoid that. A more comprehensive fix would be
  // to have Necko provide an nsIURI to PSM and to use that here (and
  // everywhere). However, that would be a wide-spanning change.
  if (net_IsValidIPv6Addr(aHostname)) {
    aOverrideAllowed = true;
    return NS_OK;
  }

  // If this is an HTTP Strict Transport Security host or a pinned host and the
  // certificate is bad, don't allow overrides (RFC 6797 section 12.1,
  // HPKP draft spec section 2.6).
  bool strictTransportSecurityEnabled = false;
  bool hasPinningInformation = false;
  nsCOMPtr<nsISiteSecurityService> sss(do_GetService(NS_SSSERVICE_CONTRACTID));
  if (!sss) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[0x%" PRIx64
             "] Couldn't get nsISiteSecurityService to check HSTS/HPKP",
             aPtrForLog));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                          NS_LITERAL_CSTRING("https://") + aHostname);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[0x%" PRIx64 "] Creating new URI failed", aPtrForLog));
    return rv;
  }

  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, uri,
                        aProviderFlags, aOriginAttributes, nullptr, nullptr,
                        &strictTransportSecurityEnabled);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[0x%" PRIx64 "] checking for HSTS failed", aPtrForLog));
    return rv;
  }

  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HPKP, uri,
                        aProviderFlags, aOriginAttributes, nullptr, nullptr,
                        &hasPinningInformation);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[0x%" PRIx64 "] checking for HPKP failed", aPtrForLog));
    return rv;
  }

  aOverrideAllowed = !strictTransportSecurityEnabled && !hasPinningInformation;
  return NS_OK;
}

void NotifyCertProblemRunnable::RunOnTargetThread() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[0x%" PRIx64 "][%p] NotifyCertProblemRunnable::RunOnTargetThread\n",
           mFdForLogging, this));
  // "Use" mFdForLogging in non-PR_LOGGING builds, too, to suppress
  // clang's -Wunused-private-field build warning for this variable:
  Unused << mFdForLogging;

  if (!NS_IsMainThread()) {
    return;
  }

  nsAutoCString hostWithPortString(mInfoObject->GetHostName());
  hostWithPortString.Append(':');
  hostWithPortString.AppendInt(mInfoObject->GetPort());

  // Try to get a nsIBadCertListener2 implementation from the socket consumer.
  nsCOMPtr<nsISSLSocketControl> sslSocketControl = do_QueryInterface(
      NS_ISUPPORTS_CAST(nsITransportSecurityInfo*, mInfoObject));
  if (sslSocketControl) {
    nsCOMPtr<nsIInterfaceRequestor> cb;
    sslSocketControl->GetNotificationCallbacks(getter_AddRefs(cb));
    if (cb) {
      nsCOMPtr<nsIBadCertListener2> bcl = do_GetInterface(cb);
      if (bcl) {
        nsIInterfaceRequestor* csi =
            static_cast<nsIInterfaceRequestor*>(mInfoObject);
        bool suppressMessage = false;  // obsolete, ignored
        Unused << bcl->NotifyCertProblem(csi, mInfoObject, hostWithPortString,
                                         &suppressMessage);
      }
    }
  }
}

// When doing async cert processing, we dispatch one of these runnables to the
// socket transport service thread, which blocks the socket transport
// service thread while it waits for the inner NotifyCertProblemRunnable to
// be executed on the main thread.  NotifyCertProblemRunnable::RunOnTargetThread
// must block events on both of these threads because it calls
// TransportSecurityInfo::GetInterface(), which may call
// nsHttpConnection::GetInterface() through TransportSecurityInfo::mCallbacks.
// nsHttpConnection::GetInterface must always execute on the main thread, with
// the socket transport service thread blocked.
class NotifyCertProblemRunnableRunnable : public Runnable {
 public:
  explicit NotifyCertProblemRunnableRunnable(
      NotifyCertProblemRunnable* aRunnable)
      : Runnable("psm::NotifyCertProblemRunnableRunnable"),
        mNotifyCertProblemRunnable(aRunnable) {}

 private:
  NS_IMETHOD Run() override {
    return mNotifyCertProblemRunnable->DispatchToMainThreadAndWait();
  }
  RefPtr<NotifyCertProblemRunnable> mNotifyCertProblemRunnable;
};

class SSLServerCertVerificationJob : public Runnable {
 public:
  // Must be called only on the socket transport thread
  static SECStatus Dispatch(const RefPtr<SharedCertVerifier>& certVerifier,
                            const void* fdForLogging,
                            TransportSecurityInfo* infoObject,
                            const UniqueCERTCertificate& serverCert,
                            const UniqueCERTCertList& peerCertChain,
                            Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
                            Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension,
                            uint32_t providerFlags, Time time, PRTime prtime,
                            uint32_t certVerifierFlags);

 private:
  NS_DECL_NSIRUNNABLE

  // Must be called only on the socket transport thread
  SSLServerCertVerificationJob(const RefPtr<SharedCertVerifier>& certVerifier,
                               const void* fdForLogging,
                               TransportSecurityInfo* infoObject,
                               const UniqueCERTCertificate& cert,
                               UniqueCERTCertList peerCertChain,
                               Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
                               Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension,
                               uint32_t providerFlags, Time time, PRTime prtime,
                               uint32_t certVerifierFlags);
  const RefPtr<SharedCertVerifier> mCertVerifier;
  const void* const mFdForLogging;
  const RefPtr<TransportSecurityInfo> mInfoObject;
  const UniqueCERTCertificate mCert;
  UniqueCERTCertList mPeerCertChain;
  const uint32_t mProviderFlags;
  const uint32_t mCertVerifierFlags;
  const Time mTime;
  const PRTime mPRTime;
  Maybe<nsTArray<uint8_t>> mStapledOCSPResponse;
  Maybe<nsTArray<uint8_t>> mSCTsFromTLSExtension;
};

SSLServerCertVerificationJob::SSLServerCertVerificationJob(
    const RefPtr<SharedCertVerifier>& certVerifier, const void* fdForLogging,
    TransportSecurityInfo* infoObject, const UniqueCERTCertificate& cert,
    UniqueCERTCertList peerCertChain,
    Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
    Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension, uint32_t providerFlags,
    Time time, PRTime prtime, uint32_t certVerifierFlags)
    : Runnable("psm::SSLServerCertVerificationJob"),
      mCertVerifier(certVerifier),
      mFdForLogging(fdForLogging),
      mInfoObject(infoObject),
      mCert(CERT_DupCertificate(cert.get())),
      mPeerCertChain(std::move(peerCertChain)),
      mProviderFlags(providerFlags),
      mCertVerifierFlags(certVerifierFlags),
      mTime(time),
      mPRTime(prtime),
      mStapledOCSPResponse(std::move(stapledOCSPResponse)),
      mSCTsFromTLSExtension(std::move(sctsFromTLSExtension)) {}

// This function assumes that we will only use the SPDY connection coalescing
// feature on connections where we have negotiated SPDY using NPN. If we ever
// talk SPDY without having negotiated it with SPDY, this code will give wrong
// and perhaps unsafe results.
//
// Returns SECSuccess on the initial handshake of all connections, on
// renegotiations for any connections where we did not negotiate SPDY, or on any
// SPDY connection where the server's certificate did not change.
//
// Prohibit changing the server cert only if we negotiated SPDY,
// in order to support SPDY's cross-origin connection pooling.
static SECStatus BlockServerCertChangeForSpdy(
    nsNSSSocketInfo* infoObject, const UniqueCERTCertificate& serverCert) {
  // Get the existing cert. If there isn't one, then there is
  // no cert change to worry about.
  nsCOMPtr<nsIX509Cert> cert;

  if (!infoObject->IsHandshakeCompleted()) {
    // first handshake on this connection, not a
    // renegotiation.
    return SECSuccess;
  }

  infoObject->GetServerCert(getter_AddRefs(cert));
  if (!cert) {
    MOZ_ASSERT_UNREACHABLE(
        "TransportSecurityInfo must have a cert implementing nsIX509Cert");
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  // Filter out sockets that did not neogtiate SPDY via NPN
  nsAutoCString negotiatedNPN;
  nsresult rv = infoObject->GetNegotiatedNPN(negotiatedNPN);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "GetNegotiatedNPN() failed during renegotiation");

  if (NS_SUCCEEDED(rv) &&
      !StringBeginsWith(negotiatedNPN, NS_LITERAL_CSTRING("spdy/"))) {
    return SECSuccess;
  }
  // If GetNegotiatedNPN() failed we will assume spdy for safety's safe
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("BlockServerCertChangeForSpdy failed GetNegotiatedNPN() call."
             " Assuming spdy.\n"));
  }

  // Check to see if the cert has actually changed
  UniqueCERTCertificate c(cert->GetCert());
  MOZ_ASSERT(c, "Somehow couldn't get underlying cert from nsIX509Cert");
  bool sameCert = CERT_CompareCerts(c.get(), serverCert.get());
  if (sameCert) {
    return SECSuccess;
  }

  // Report an error - changed cert is confirmed
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("SPDY Refused to allow new cert during renegotiation\n"));
  PR_SetError(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED, 0);
  return SECFailure;
}

void AccumulateSubjectCommonNameTelemetry(const char* commonName,
                                          bool commonNameInSubjectAltNames) {
  if (!commonName) {
    // 1 means no common name present
    Telemetry::Accumulate(Telemetry::BR_9_2_2_SUBJECT_COMMON_NAME, 1);
  } else if (!commonNameInSubjectAltNames) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("BR telemetry: common name '%s' not in subject alt. names "
             "(or the subject alt. names extension is not present)\n",
             commonName));
    // 2 means the common name is not present in subject alt names
    Telemetry::Accumulate(Telemetry::BR_9_2_2_SUBJECT_COMMON_NAME, 2);
  } else {
    // 0 means the common name is present in subject alt names
    Telemetry::Accumulate(Telemetry::BR_9_2_2_SUBJECT_COMMON_NAME, 0);
  }
}

// Returns true if and only if commonName ends with altName (minus its leading
// "*"). altName has already been checked to be of the form "*.<something>".
// commonName may be NULL.
static bool TryMatchingWildcardSubjectAltName(const char* commonName,
                                              const nsACString& altName) {
  return commonName &&
         StringEndsWith(nsDependentCString(commonName), Substring(altName, 1));
}

// Gathers telemetry on Baseline Requirements 9.2.1 (Subject Alternative
// Names Extension) and 9.2.2 (Subject Common Name Field).
// Specifically:
//  - whether or not the subject common name field is present
//  - whether or not the subject alternative names extension is present
//  - if there is a malformed entry in the subject alt. names extension
//  - if there is an entry in the subject alt. names extension corresponding
//    to the subject common name
// Telemetry is only gathered for certificates that chain to a trusted root
// in Mozilla's Root CA program.
// certList consists of a validated certificate chain. The end-entity
// certificate is first and the root (trust anchor) is last.
void GatherBaselineRequirementsTelemetry(const UniqueCERTCertList& certList) {
  CERTCertListNode* endEntityNode = CERT_LIST_HEAD(certList);
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  MOZ_ASSERT(!(CERT_LIST_END(endEntityNode, certList) ||
               CERT_LIST_END(rootNode, certList)));
  if (CERT_LIST_END(endEntityNode, certList) ||
      CERT_LIST_END(rootNode, certList)) {
    return;
  }
  CERTCertificate* cert = endEntityNode->cert;
  MOZ_ASSERT(cert);
  if (!cert) {
    return;
  }
  UniquePORTString commonName(CERT_GetCommonName(&cert->subject));
  // This only applies to certificates issued by authorities in our root
  // program.
  CERTCertificate* rootCert = rootNode->cert;
  MOZ_ASSERT(rootCert);
  if (!rootCert) {
    return;
  }
  bool isBuiltIn = false;
  Result result = IsCertBuiltInRoot(rootCert, isBuiltIn);
  if (result != Success || !isBuiltIn) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("BR telemetry: root certificate for '%s' is not a built-in root "
             "(or IsCertBuiltInRoot failed)\n",
             commonName.get()));
    return;
  }
  ScopedAutoSECItem altNameExtension;
  SECStatus rv = CERT_FindCertExtension(cert, SEC_OID_X509_SUBJECT_ALT_NAME,
                                        &altNameExtension);
  if (rv != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("BR telemetry: no subject alt names extension for '%s'\n",
             commonName.get()));
    // 1 means there is no subject alt names extension
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 1);
    AccumulateSubjectCommonNameTelemetry(commonName.get(), false);
    return;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CERTGeneralName* subjectAltNames =
      CERT_DecodeAltNameExtension(arena.get(), &altNameExtension);
  if (!subjectAltNames) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("BR telemetry: could not decode subject alt names for '%s'\n",
             commonName.get()));
    // 2 means the subject alt names extension could not be decoded
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 2);
    AccumulateSubjectCommonNameTelemetry(commonName.get(), false);
    return;
  }

  CERTGeneralName* currentName = subjectAltNames;
  bool commonNameInSubjectAltNames = false;
  bool nonDNSNameOrIPAddressPresent = false;
  bool malformedDNSNameOrIPAddressPresent = false;
  bool nonFQDNPresent = false;
  do {
    nsAutoCString altName;
    if (currentName->type == certDNSName) {
      altName.Assign(
          BitwiseCast<char*, unsigned char*>(currentName->name.other.data),
          currentName->name.other.len);
      nsDependentCString altNameWithoutWildcard(altName, 0);
      if (StringBeginsWith(altNameWithoutWildcard, NS_LITERAL_CSTRING("*."))) {
        altNameWithoutWildcard.Rebind(altName, 2);
        commonNameInSubjectAltNames |=
            TryMatchingWildcardSubjectAltName(commonName.get(), altName);
      }
      // net_IsValidHostName appears to return true for valid IP addresses,
      // which would be invalid for a DNS name.
      // Note that the net_IsValidHostName check will catch things like
      // "a.*.example.com".
      if (!net_IsValidHostName(altNameWithoutWildcard) ||
          net_IsValidIPv4Addr(altName) || net_IsValidIPv6Addr(altName)) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("BR telemetry: DNSName '%s' not valid (for '%s')\n",
                 altName.get(), commonName.get()));
        malformedDNSNameOrIPAddressPresent = true;
      }
      if (!altName.Contains('.')) {
        nonFQDNPresent = true;
      }
    } else if (currentName->type == certIPAddress) {
      // According to DNS.h, this includes space for the null-terminator
      char buf[net::kNetAddrMaxCStrBufSize] = {0};
      PRNetAddr addr;
      if (currentName->name.other.len == 4) {
        addr.inet.family = PR_AF_INET;
        memcpy(&addr.inet.ip, currentName->name.other.data,
               currentName->name.other.len);
        if (PR_NetAddrToString(&addr, buf, sizeof(buf) - 1) != PR_SUCCESS) {
          MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                  ("BR telemetry: IPAddress (v4) not valid (for '%s')\n",
                   commonName.get()));
          malformedDNSNameOrIPAddressPresent = true;
        } else {
          altName.Assign(buf);
        }
      } else if (currentName->name.other.len == 16) {
        addr.inet.family = PR_AF_INET6;
        memcpy(&addr.ipv6.ip, currentName->name.other.data,
               currentName->name.other.len);
        if (PR_NetAddrToString(&addr, buf, sizeof(buf) - 1) != PR_SUCCESS) {
          MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                  ("BR telemetry: IPAddress (v6) not valid (for '%s')\n",
                   commonName.get()));
          malformedDNSNameOrIPAddressPresent = true;
        } else {
          altName.Assign(buf);
        }
      } else {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("BR telemetry: IPAddress not valid (for '%s')\n",
                 commonName.get()));
        malformedDNSNameOrIPAddressPresent = true;
      }
    } else {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("BR telemetry: non-DNSName, non-IPAddress present for '%s'\n",
               commonName.get()));
      nonDNSNameOrIPAddressPresent = true;
    }
    if (commonName && altName.Equals(commonName.get())) {
      commonNameInSubjectAltNames = true;
    }
    currentName = CERT_GetNextGeneralName(currentName);
  } while (currentName && currentName != subjectAltNames);

  if (nonDNSNameOrIPAddressPresent) {
    // 3 means there's an entry that isn't an ip address or dns name
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 3);
  }
  if (malformedDNSNameOrIPAddressPresent) {
    // 4 means there's a malformed ip address or dns name entry
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 4);
  }
  if (nonFQDNPresent) {
    // 5 means there's a DNS name entry with a non-fully-qualified domain name
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 5);
  }
  if (!nonDNSNameOrIPAddressPresent && !malformedDNSNameOrIPAddressPresent &&
      !nonFQDNPresent) {
    // 0 means the extension is acceptable
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 0);
  }

  AccumulateSubjectCommonNameTelemetry(commonName.get(),
                                       commonNameInSubjectAltNames);
}

// Gather telemetry on whether the end-entity cert for a server has the
// required TLS Server Authentication EKU, or any others
void GatherEKUTelemetry(const UniqueCERTCertList& certList) {
  CERTCertListNode* endEntityNode = CERT_LIST_HEAD(certList);
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  MOZ_ASSERT(!(CERT_LIST_END(endEntityNode, certList) ||
               CERT_LIST_END(rootNode, certList)));
  if (CERT_LIST_END(endEntityNode, certList) ||
      CERT_LIST_END(rootNode, certList)) {
    return;
  }
  CERTCertificate* endEntityCert = endEntityNode->cert;
  MOZ_ASSERT(endEntityCert);
  if (!endEntityCert) {
    return;
  }

  // Only log telemetry if the root CA is built-in
  CERTCertificate* rootCert = rootNode->cert;
  MOZ_ASSERT(rootCert);
  if (!rootCert) {
    return;
  }
  bool isBuiltIn = false;
  Result rv = IsCertBuiltInRoot(rootCert, isBuiltIn);
  if (rv != Success || !isBuiltIn) {
    return;
  }

  // Find the EKU extension, if present
  bool foundEKU = false;
  SECOidTag oidTag;
  CERTCertExtension* ekuExtension = nullptr;
  for (size_t i = 0; endEntityCert->extensions && endEntityCert->extensions[i];
       i++) {
    oidTag = SECOID_FindOIDTag(&endEntityCert->extensions[i]->id);
    if (oidTag == SEC_OID_X509_EXT_KEY_USAGE) {
      foundEKU = true;
      ekuExtension = endEntityCert->extensions[i];
    }
  }

  if (!foundEKU) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 0);
    return;
  }

  // Parse the EKU extension
  UniqueCERTOidSequence ekuSequence(
      CERT_DecodeOidSequence(&ekuExtension->value));
  if (!ekuSequence) {
    return;
  }

  // Search through the available EKUs
  bool foundServerAuth = false;
  bool foundOther = false;
  for (SECItem** oids = ekuSequence->oids; oids && *oids; oids++) {
    oidTag = SECOID_FindOIDTag(*oids);
    if (oidTag == SEC_OID_EXT_KEY_USAGE_SERVER_AUTH) {
      foundServerAuth = true;
    } else {
      foundOther = true;
    }
  }

  // Cases 3 is included only for completeness.  It should never
  // appear in these statistics, because CheckExtendedKeyUsage()
  // should require the EKU extension, if present, to contain the
  // value id_kp_serverAuth.
  if (foundServerAuth && !foundOther) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 1);
  } else if (foundServerAuth && foundOther) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 2);
  } else if (!foundServerAuth) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 3);
  }
}

// Gathers telemetry on which CA is the root of a given cert chain.
// If the root is a built-in root, then the telemetry makes a count
// by root.  Roots that are not built-in are counted in one bin.
void GatherRootCATelemetry(const UniqueCERTCertList& certList) {
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  MOZ_ASSERT(rootNode);
  if (!rootNode) {
    return;
  }
  MOZ_ASSERT(!CERT_LIST_END(rootNode, certList));
  if (CERT_LIST_END(rootNode, certList)) {
    return;
  }
  CERTCertificate* rootCert = rootNode->cert;
  MOZ_ASSERT(rootCert);
  if (!rootCert) {
    return;
  }
  AccumulateTelemetryForRootCA(Telemetry::CERT_VALIDATION_SUCCESS_BY_CA,
                               rootCert);
}

// There are various things that we want to measure about certificate
// chains that we accept.  This is a single entry point for all of them.
void GatherSuccessfulValidationTelemetry(const UniqueCERTCertList& certList) {
  GatherBaselineRequirementsTelemetry(certList);
  GatherEKUTelemetry(certList);
  GatherRootCATelemetry(certList);
}

void GatherTelemetryForSingleSCT(const ct::VerifiedSCT& verifiedSct) {
  // See SSL_SCTS_ORIGIN in Histograms.json.
  uint32_t origin = 0;
  switch (verifiedSct.origin) {
    case ct::VerifiedSCT::Origin::Embedded:
      origin = 1;
      break;
    case ct::VerifiedSCT::Origin::TLSExtension:
      origin = 2;
      break;
    case ct::VerifiedSCT::Origin::OCSPResponse:
      origin = 3;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected VerifiedSCT::Origin type");
  }
  Telemetry::Accumulate(Telemetry::SSL_SCTS_ORIGIN, origin);

  // See SSL_SCTS_VERIFICATION_STATUS in Histograms.json.
  uint32_t verificationStatus = 0;
  switch (verifiedSct.status) {
    case ct::VerifiedSCT::Status::Valid:
      verificationStatus = 1;
      break;
    case ct::VerifiedSCT::Status::UnknownLog:
      verificationStatus = 2;
      break;
    case ct::VerifiedSCT::Status::InvalidSignature:
      verificationStatus = 3;
      break;
    case ct::VerifiedSCT::Status::InvalidTimestamp:
      verificationStatus = 4;
      break;
    case ct::VerifiedSCT::Status::ValidFromDisqualifiedLog:
      verificationStatus = 5;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected VerifiedSCT::Status type");
  }
  Telemetry::Accumulate(Telemetry::SSL_SCTS_VERIFICATION_STATUS,
                        verificationStatus);
}

void GatherCertificateTransparencyTelemetry(
    const UniqueCERTCertList& certList, bool isEV,
    const CertificateTransparencyInfo& info) {
  if (!info.enabled) {
    // No telemetry is gathered when CT is disabled.
    return;
  }

  for (const ct::VerifiedSCT& sct : info.verifyResult.verifiedScts) {
    GatherTelemetryForSingleSCT(sct);
  }

  // Decoding errors are reported to the 0th bucket
  // of the SSL_SCTS_VERIFICATION_STATUS enumerated probe.
  for (size_t i = 0; i < info.verifyResult.decodingErrors; ++i) {
    Telemetry::Accumulate(Telemetry::SSL_SCTS_VERIFICATION_STATUS, 0);
  }

  // Handle the histogram of SCTs counts.
  uint32_t sctsCount =
      static_cast<uint32_t>(info.verifyResult.verifiedScts.size());
  // Note that sctsCount can also be 0 in case we've received SCT binary data,
  // but it failed to parse (e.g. due to unsupported CT protocol version).
  Telemetry::Accumulate(Telemetry::SSL_SCTS_PER_CONNECTION, sctsCount);

  // Report CT Policy compliance of EV certificates.
  if (isEV) {
    uint32_t evCompliance = 0;
    switch (info.policyCompliance) {
      case ct::CTPolicyCompliance::Compliant:
        evCompliance = 1;
        break;
      case ct::CTPolicyCompliance::NotEnoughScts:
        evCompliance = 2;
        break;
      case ct::CTPolicyCompliance::NotDiverseScts:
        evCompliance = 3;
        break;
      case ct::CTPolicyCompliance::Unknown:
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected CTPolicyCompliance type");
    }
    Telemetry::Accumulate(Telemetry::SSL_CT_POLICY_COMPLIANCE_OF_EV_CERTS,
                          evCompliance);
  }

  // Get the root cert.
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  MOZ_ASSERT(rootNode);
  if (!rootNode) {
    return;
  }
  MOZ_ASSERT(!CERT_LIST_END(rootNode, certList));
  if (CERT_LIST_END(rootNode, certList)) {
    return;
  }
  CERTCertificate* rootCert = rootNode->cert;
  MOZ_ASSERT(rootCert);
  if (!rootCert) {
    return;
  }

  // Report CT Policy compliance by CA.
  switch (info.policyCompliance) {
    case ct::CTPolicyCompliance::Compliant:
      AccumulateTelemetryForRootCA(
          Telemetry::SSL_CT_POLICY_COMPLIANT_CONNECTIONS_BY_CA, rootCert);
      break;
    case ct::CTPolicyCompliance::NotEnoughScts:
    case ct::CTPolicyCompliance::NotDiverseScts:
      AccumulateTelemetryForRootCA(
          Telemetry::SSL_CT_POLICY_NON_COMPLIANT_CONNECTIONS_BY_CA, rootCert);
      break;
    case ct::CTPolicyCompliance::Unknown:
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected CTPolicyCompliance type");
  }
}

// This function collects telemetry about certs. It will be called on one of
// CertVerificationThread. When the socket process is used this will be called
// on the parent process.
static void CollectCertTelemetry(
    mozilla::pkix::Result aCertVerificationResult, SECOidTag aEvOidPolicy,
    CertVerifier::OCSPStaplingStatus aOcspStaplingStatus,
    KeySizeStatus aKeySizeStatus, SHA1ModeResult aSha1ModeResult,
    const PinningTelemetryInfo& aPinningTelemetryInfo,
    const UniqueCERTCertList& aBuiltCertChain,
    const CertificateTransparencyInfo& aCertificateTransparencyInfo) {
  uint32_t evStatus = (aCertVerificationResult != Success)
                          ? 0  // 0 = Failure
                          : (aEvOidPolicy == SEC_OID_UNKNOWN) ? 1   // 1 = DV
                                                              : 2;  // 2 = EV
  Telemetry::Accumulate(Telemetry::CERT_EV_STATUS, evStatus);

  if (aOcspStaplingStatus != CertVerifier::OCSP_STAPLING_NEVER_CHECKED) {
    Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, aOcspStaplingStatus);
  }

  if (aKeySizeStatus != KeySizeStatus::NeverChecked) {
    Telemetry::Accumulate(Telemetry::CERT_CHAIN_KEY_SIZE_STATUS,
                          static_cast<uint32_t>(aKeySizeStatus));
  }

  if (aSha1ModeResult != SHA1ModeResult::NeverChecked) {
    Telemetry::Accumulate(Telemetry::CERT_CHAIN_SHA1_POLICY_STATUS,
                          static_cast<uint32_t>(aSha1ModeResult));
  }

  if (aPinningTelemetryInfo.accumulateForRoot) {
    Telemetry::Accumulate(Telemetry::CERT_PINNING_FAILURES_BY_CA,
                          aPinningTelemetryInfo.rootBucket);
  }

  if (aPinningTelemetryInfo.accumulateResult) {
    MOZ_ASSERT(aPinningTelemetryInfo.certPinningResultHistogram.isSome());
    Telemetry::Accumulate(
        aPinningTelemetryInfo.certPinningResultHistogram.value(),
        aPinningTelemetryInfo.certPinningResultBucket);
  }

  if (aCertVerificationResult == Success) {
    GatherSuccessfulValidationTelemetry(aBuiltCertChain);
    GatherCertificateTransparencyTelemetry(
        aBuiltCertChain,
        /*isEV*/ aEvOidPolicy != SEC_OID_UNKNOWN, aCertificateTransparencyInfo);
  }
}

static void AuthCertificateSetResults(
    TransportSecurityInfo* aInfoObject, const UniqueCERTCertificate& aCert,
    UniqueCERTCertList& aBuiltCertChain, UniqueCERTCertList& aPeerCertChain,
    const CertificateTransparencyInfo& aCertificateTransparencyInfo,
    SECOidTag aEvOidPolicy, bool aSucceeded) {
  MOZ_ASSERT(aInfoObject);

  if (aSucceeded) {
    // Certificate verification succeeded. Delete any potential record of
    // certificate error bits.
    RememberCertErrorsTable::GetInstance().RememberCertHasError(aInfoObject,
                                                                SECSuccess);

    EVStatus evStatus;
    if (aEvOidPolicy == SEC_OID_UNKNOWN) {
      evStatus = EVStatus::NotEV;
    } else {
      evStatus = EVStatus::EV;
    }

    RefPtr<nsNSSCertificate> nsc = nsNSSCertificate::Create(aCert.get());
    aInfoObject->SetServerCert(nsc, evStatus);

    aInfoObject->SetSucceededCertChain(std::move(aBuiltCertChain));
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("AuthCertificate setting NEW cert %p", nsc.get()));

    aInfoObject->SetCertificateTransparencyInfo(aCertificateTransparencyInfo);
  } else {
    // Certificate validation failed; store the peer certificate chain on
    // infoObject so it can be used for error reporting.
    aInfoObject->SetFailedCertChain(std::move(aPeerCertChain));
  }
}

// Note: Takes ownership of |peerCertChain| if SECSuccess is not returned.
Result AuthCertificate(CertVerifier& certVerifier,
                       TransportSecurityInfo* infoObject,
                       const UniqueCERTCertificate& cert,
                       UniqueCERTCertList& peerCertChain,
                       const Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
                       const Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension,
                       uint32_t providerFlags, Time time,
                       uint32_t certVerifierFlags) {
  MOZ_ASSERT(infoObject);
  MOZ_ASSERT(cert);

  // We want to avoid storing any intermediate cert information when browsing
  // in private, transient contexts.
  bool saveIntermediates =
      !(providerFlags & nsISocketProvider::NO_PERMANENT_STORAGE);

  SECOidTag evOidPolicy;
  UniqueCERTCertList builtCertChain;
  CertVerifier::OCSPStaplingStatus ocspStaplingStatus =
      CertVerifier::OCSP_STAPLING_NEVER_CHECKED;
  KeySizeStatus keySizeStatus = KeySizeStatus::NeverChecked;
  SHA1ModeResult sha1ModeResult = SHA1ModeResult::NeverChecked;
  PinningTelemetryInfo pinningTelemetryInfo;
  CertificateTransparencyInfo certificateTransparencyInfo;

  Result rv = certVerifier.VerifySSLServerCert(
      cert, stapledOCSPResponse, sctsFromTLSExtension, time, infoObject,
      infoObject->GetHostName(), builtCertChain, saveIntermediates,
      certVerifierFlags, infoObject->GetOriginAttributes(), &evOidPolicy,
      &ocspStaplingStatus, &keySizeStatus, &sha1ModeResult,
      &pinningTelemetryInfo, &certificateTransparencyInfo);

  CollectCertTelemetry(rv, evOidPolicy, ocspStaplingStatus, keySizeStatus,
                       sha1ModeResult, pinningTelemetryInfo, builtCertChain,
                       certificateTransparencyInfo);

  AuthCertificateSetResults(infoObject, cert, builtCertChain, peerCertChain,
                            certificateTransparencyInfo, evOidPolicy,
                            rv == Success);
  return rv;
}

/*static*/
SECStatus SSLServerCertVerificationJob::Dispatch(
    const RefPtr<SharedCertVerifier>& certVerifier, const void* fdForLogging,
    TransportSecurityInfo* infoObject, const UniqueCERTCertificate& serverCert,
    const UniqueCERTCertList& peerCertChain,
    Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
    Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension, uint32_t providerFlags,
    Time time, PRTime prtime, uint32_t certVerifierFlags) {
  // Runs on the socket transport thread
  if (!certVerifier || !infoObject || !serverCert) {
    NS_ERROR("Invalid parameters for SSL server cert validation");
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  if (!gCertVerificationThreadPool) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // Copy the certificate list so the runnable can take ownership of it in the
  // constructor.
  UniqueCERTCertList peerCertChainCopy =
      nsNSSCertList::DupCertList(peerCertChain);
  if (!peerCertChainCopy) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return SECFailure;
  }

  RefPtr<SSLServerCertVerificationJob> job(new SSLServerCertVerificationJob(
      certVerifier, fdForLogging, infoObject, serverCert,
      std::move(peerCertChainCopy), stapledOCSPResponse, sctsFromTLSExtension,
      providerFlags, time, prtime, certVerifierFlags));

  nsresult nrv = gCertVerificationThreadPool->Dispatch(job, NS_DISPATCH_NORMAL);
  if (NS_FAILED(nrv)) {
    // We can't call SetCertVerificationResult here to change
    // mCertVerificationState because SetCertVerificationResult will call
    // libssl functions that acquire SSL locks that are already being held at
    // this point. However, we can set an error with PR_SetError and return
    // SECFailure, and the correct thing will happen (the error will be
    // propagated and this connection will be terminated).
    PRErrorCode error = nrv == NS_ERROR_OUT_OF_MEMORY ? PR_OUT_OF_MEMORY_ERROR
                                                      : PR_INVALID_STATE_ERROR;
    PR_SetError(error, 0);
    return SECFailure;
  }

  PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
  return SECWouldBlock;
}

PRErrorCode AuthCertificateParseResults(
    uint64_t aPtrForLog, const nsACString& aHostName, int32_t aPort,
    const OriginAttributes& aOriginAttributes,
    const UniqueCERTCertificate& aCert, uint32_t aProviderFlags, PRTime aPRTime,
    PRErrorCode aDefaultErrorCodeToReport,
    /* out */ uint32_t& aCollectedErrors) {
  if (aDefaultErrorCodeToReport == 0) {
    MOZ_ASSERT_UNREACHABLE(
        "No error set during certificate validation failure");
    return SEC_ERROR_LIBRARY_FAILURE;
  }

  uint32_t probeValue = MapCertErrorToProbeValue(aDefaultErrorCodeToReport);
  Telemetry::Accumulate(Telemetry::SSL_CERT_VERIFICATION_ERRORS, probeValue);

  aCollectedErrors = 0;
  PRErrorCode errorCodeTrust = 0;
  PRErrorCode errorCodeMismatch = 0;
  PRErrorCode errorCodeTime = 0;
  if (DetermineCertOverrideErrors(aCert, aHostName, aPRTime,
                                  aDefaultErrorCodeToReport, aCollectedErrors,
                                  errorCodeTrust, errorCodeMismatch,
                                  errorCodeTime) != SECSuccess) {
    PRErrorCode errorCode = PR_GetError();
    MOZ_ASSERT(!ErrorIsOverridable(errorCode));
    if (errorCode == 0) {
      MOZ_ASSERT_UNREACHABLE(
          "No error set during DetermineCertOverrideErrors failure");
      return SEC_ERROR_LIBRARY_FAILURE;
    }
    return errorCode;
  }

  if (!aCollectedErrors) {
    MOZ_ASSERT_UNREACHABLE("aCollectedErrors should not be 0");
    return SEC_ERROR_LIBRARY_FAILURE;
  }

  bool overrideAllowed = false;
  if (NS_FAILED(OverrideAllowedForHost(aPtrForLog, aHostName, aOriginAttributes,
                                       aProviderFlags, overrideAllowed))) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[0x%" PRIx64 "] AuthCertificateParseResults - "
             "OverrideAllowedForHost failed\n",
             aPtrForLog));
    return aDefaultErrorCodeToReport;
  }

  if (overrideAllowed) {
    nsCOMPtr<nsICertOverrideService> overrideService =
        do_GetService(NS_CERTOVERRIDE_CONTRACTID);

    uint32_t overrideBits = 0;
    uint32_t remainingDisplayErrors = aCollectedErrors;

    // it is fine to continue without the nsICertOverrideService
    if (overrideService) {
      bool haveOverride;
      bool isTemporaryOverride;  // we don't care
      RefPtr<nsIX509Cert> nssCert(nsNSSCertificate::Create(aCert.get()));
      if (!nssCert) {
        MOZ_ASSERT(false, "nsNSSCertificate::Create failed");
        return SEC_ERROR_NO_MEMORY;
      }
      nsresult rv = overrideService->HasMatchingOverride(
          aHostName, aPort, nssCert, &overrideBits, &isTemporaryOverride,
          &haveOverride);
      if (NS_SUCCEEDED(rv) && haveOverride) {
        // remove the errors that are already overriden
        remainingDisplayErrors &= ~overrideBits;
      }
    }

    if (!remainingDisplayErrors) {
      // This can double- or triple-count one certificate with multiple
      // different types of errors. Since this is telemetry and we just
      // want a ballpark answer, we don't care.
      if (errorCodeTrust != 0) {
        uint32_t probeValue = MapOverridableErrorToProbeValue(errorCodeTrust);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }
      if (errorCodeMismatch != 0) {
        uint32_t probeValue =
            MapOverridableErrorToProbeValue(errorCodeMismatch);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }
      if (errorCodeTime != 0) {
        uint32_t probeValue = MapOverridableErrorToProbeValue(errorCodeTime);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }

      // all errors are covered by override rules, so let's accept the cert
      MOZ_LOG(
          gPIPNSSLog, LogLevel::Debug,
          ("[0x%" PRIx64 "] All errors covered by override rules", aPtrForLog));
      return 0;
    }
  } else {
    MOZ_LOG(
        gPIPNSSLog, LogLevel::Debug,
        ("[0x%" PRIx64 "] HSTS or HPKP - no overrides allowed\n", aPtrForLog));
  }

  MOZ_LOG(
      gPIPNSSLog, LogLevel::Debug,
      ("[0x%" PRIx64 "] Certificate error was not overridden\n", aPtrForLog));

  // pick the error code to report by priority
  return errorCodeTrust
             ? errorCodeTrust
             : errorCodeMismatch
                   ? errorCodeMismatch
                   : errorCodeTime ? errorCodeTime : aDefaultErrorCodeToReport;
}

NS_IMETHODIMP
SSLServerCertVerificationJob::Run() {
  // Runs on a cert verification thread and only on parent process.
  MOZ_ASSERT(XRE_IsParentProcess());

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] SSLServerCertVerificationJob::Run\n", mInfoObject.get()));

  TimeStamp jobStartTime = TimeStamp::Now();
  Result rv = AuthCertificate(
      *mCertVerifier, mInfoObject, mCert, mPeerCertChain, mStapledOCSPResponse,
      mSCTsFromTLSExtension, mProviderFlags, mTime, mCertVerifierFlags);
  MOZ_ASSERT(
      (mPeerCertChain && rv == Success) || (!mPeerCertChain && rv != Success),
      "AuthCertificate() should take ownership of chain on failure");

  if (rv == Success) {
    Telemetry::AccumulateTimeDelta(
        Telemetry::SSL_SUCCESFUL_CERT_VALIDATION_TIME_MOZILLAPKIX, jobStartTime,
        TimeStamp::Now());
    Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, 1);
    RefPtr<SSLServerCertVerificationResult> runnable(
        new SSLServerCertVerificationResult(mInfoObject, 0));
    runnable->Dispatch();
    return NS_OK;
  }

  Telemetry::AccumulateTimeDelta(
      Telemetry::SSL_INITIAL_FAILED_CERT_VALIDATION_TIME_MOZILLAPKIX,
      jobStartTime, TimeStamp::Now());

  PRErrorCode error = MapResultToPRErrorCode(rv);
  uint64_t addr = reinterpret_cast<uintptr_t>(mFdForLogging);
  uint32_t collectedErrors = 0;
  PRErrorCode finalError = AuthCertificateParseResults(
      addr, mInfoObject->GetHostName(), mInfoObject->GetPort(),
      mInfoObject->GetOriginAttributes(), mCert, mProviderFlags, mPRTime, error,
      collectedErrors);

  if (collectedErrors != 0) {
    RefPtr<nsNSSCertificate> nssCert(nsNSSCertificate::Create(mCert.get()));
    mInfoObject->SetStatusErrorBits(nssCert, collectedErrors);
  }

  if (finalError == 0) {
    RefPtr<SSLServerCertVerificationResult> runnable(
        new SSLServerCertVerificationResult(mInfoObject, 0));
    runnable->Dispatch();
    return NS_OK;
  }

  // Until bug 1547096 is fixed, we need to notify any nsIBadCertListener2
  // implementations of the certificate verification error.

  // Accessing nsIBadCertListener2 must happen on the main thread and at the
  // same the socket thread must be blocked.
  RefPtr<NotifyCertProblemRunnable> runnable(
      new NotifyCertProblemRunnable(addr, mInfoObject));

  // We must block the the socket transport service thread while the
  // main thread executes the NotifyCertProblemRunnable.
  // The NotifyCertProblemRunnable will dispatch the result asynchronously,
  // so we don't have to block this thread waiting for it.

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[0x%" PRIx64 "][%p] Before dispatching NotifyCertProblemRunnable\n",
           addr, runnable.get()));

  nsresult nrv;
  nsCOMPtr<nsIEventTarget> stsTarget =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &nrv);
  if (NS_SUCCEEDED(nrv)) {
    nrv = stsTarget->Dispatch(new NotifyCertProblemRunnableRunnable(runnable),
                              NS_DISPATCH_NORMAL);
  }
  if (NS_FAILED(nrv)) {
    finalError = PR_INVALID_STATE_ERROR;
  }

  RefPtr<SSLServerCertVerificationResult> resultRunnable(
      new SSLServerCertVerificationResult(mInfoObject, finalError));
  resultRunnable->Dispatch();
  return NS_OK;
}

}  // unnamed namespace

// Takes information needed for cert verification, does some consistency
//  checks and calls SSLServerCertVerificationJob::Dispatch.
SECStatus AuthCertificateHookInternal(
    TransportSecurityInfo* infoObject, const void* aPtrForLogging,
    const UniqueCERTCertificate& serverCert, UniqueCERTCertList& peerCertChain,
    Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
    Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension, uint32_t providerFlags,
    uint32_t certVerifierFlags) {
  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  if (!certVerifier) {
    PR_SetError(SEC_ERROR_NOT_INITIALIZED, 0);
    return SECFailure;
  }

  // Runs on the socket transport thread

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] starting AuthCertificateHookInternal\n", aPtrForLogging));

  if (!infoObject || !serverCert) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  bool onSTSThread;
  nsresult nrv;
  nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &nrv);
  if (NS_SUCCEEDED(nrv)) {
    nrv = sts->IsOnCurrentThread(&onSTSThread);
  }

  if (NS_FAILED(nrv)) {
    NS_ERROR("Could not get STS service or IsOnCurrentThread failed");
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return SECFailure;
  }

  MOZ_ASSERT(onSTSThread);

  if (!onSTSThread) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // We *must* do certificate verification on a background thread because
  // we need the socket transport thread to be free for our OCSP requests,
  // and we *want* to do certificate verification on a background thread
  // because of the performance benefits of doing so.
  return SSLServerCertVerificationJob::Dispatch(
      certVerifier, aPtrForLogging, infoObject, serverCert, peerCertChain,
      stapledOCSPResponse, sctsFromTLSExtension, providerFlags, Now(), PR_Now(),
      certVerifierFlags);
}

// Extracts whatever information we need out of fd (using SSL_*) and passes it
// to AuthCertificateHookInternal. AuthCertificateHookInternal will call
// SSLServerCertVerificationJob::Dispatch. SSLServerCertVerificationJob
// should never do anything with fd except logging.
SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checkSig,
                              PRBool isServer) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] starting AuthCertificateHook\n", fd));

  // Modern libssl always passes PR_TRUE for checkSig, and we have no means of
  // doing verification without checking signatures.
  MOZ_ASSERT(checkSig, "AuthCertificateHook: checkSig unexpectedly false");

  // PSM never causes libssl to call this function with PR_TRUE for isServer,
  // and many things in PSM assume that we are a client.
  MOZ_ASSERT(!isServer, "AuthCertificateHook: isServer unexpectedly true");

  nsNSSSocketInfo* socketInfo = static_cast<nsNSSSocketInfo*>(arg);

  UniqueCERTCertificate serverCert(SSL_PeerCertificate(fd));

  if (!checkSig || isServer || !socketInfo || !serverCert) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }
  socketInfo->SetFullHandshake();

  if (BlockServerCertChangeForSpdy(socketInfo, serverCert) != SECSuccess) {
    return SECFailure;
  }

  // Get the peer certificate chain for error reporting
  UniqueCERTCertList peerCertChain(SSL_PeerCertificateChain(fd));
  if (!peerCertChain) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // SSL_PeerStapledOCSPResponses will never return a non-empty response if
  // OCSP stapling wasn't enabled because libssl wouldn't have let the server
  // return a stapled OCSP response.
  // We don't own these pointers.
  const SECItemArray* csa = SSL_PeerStapledOCSPResponses(fd);
  Maybe<nsTArray<uint8_t>> stapledOCSPResponse;
  // we currently only support single stapled responses
  if (csa && csa->len == 1) {
    stapledOCSPResponse.emplace();
    stapledOCSPResponse->SetCapacity(csa->items[0].len);
    stapledOCSPResponse->AppendElements(csa->items[0].data, csa->items[0].len);
  }

  Maybe<nsTArray<uint8_t>> sctsFromTLSExtension;
  const SECItem* sctsFromTLSExtensionSECItem = SSL_PeerSignedCertTimestamps(fd);
  if (sctsFromTLSExtensionSECItem) {
    sctsFromTLSExtension.emplace();
    sctsFromTLSExtension->SetCapacity(sctsFromTLSExtensionSECItem->len);
    sctsFromTLSExtension->AppendElements(sctsFromTLSExtensionSECItem->data,
                                         sctsFromTLSExtensionSECItem->len);
  }

  uint32_t providerFlags = 0;
  socketInfo->GetProviderFlags(&providerFlags);

  uint32_t certVerifierFlags = 0;
  if (!socketInfo->SharedState().IsOCSPStaplingEnabled() ||
      !socketInfo->SharedState().IsOCSPMustStapleEnabled()) {
    certVerifierFlags |= CertVerifier::FLAG_TLS_IGNORE_STATUS_REQUEST;
  }

  socketInfo->SetCertVerificationWaiting();
  return AuthCertificateHookInternal(socketInfo, static_cast<const void*>(fd),
                                     serverCert, peerCertChain,
                                     stapledOCSPResponse, sctsFromTLSExtension,
                                     providerFlags, certVerifierFlags);
}

// Make a cert chain from an array of ders.
SECStatus MakeCertChain(nsTArray<nsTArray<uint8_t>>& inPeerCertChain,
                        UniqueCERTCertificate& outCert,
                        UniqueCERTCertList& outCertChain) {
  for (auto& certDer : inPeerCertChain) {
    if (certDer.Length() > UINT32_MAX) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("MakeCertChain: certDer too long."));
      return SECFailure;
    }
    SECItem der = {SECItemType::siBuffer, certDer.Elements(),
                   (uint32_t)certDer.Length()};

    if (!outCert) {
      outCert.reset(CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der,
                                            nullptr, false, true));
      if (!outCert) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("MakeCertChain: cert failed"));
        return SECFailure;
      }
    }

    UniqueCERTCertificate tmpCert(CERT_NewTempCertificate(
        CERT_GetDefaultCertDB(), &der, nullptr, false, true));
    if (!tmpCert) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("MakeCertChain: cert failed"));
      return SECFailure;
    }
    if (CERT_AddCertToListTail(outCertChain.get(), tmpCert.get()) !=
        SECSuccess) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("MakeCertChain: getting cert chain failed"));
      return SECFailure;
    }
    Unused << tmpCert.release();
  }
  return SECSuccess;
}

// Takes information needed for cert verification, does some consistency
// checks and calls SSLServerCertVerificationJob::Dispatch.
// This function is used for Quic.
SECStatus AuthCertificateHookWithInfo(
    TransportSecurityInfo* infoObject, const void* aPtrForLogging,
    nsTArray<nsTArray<uint8_t>>& peerCertChain,
    Maybe<nsTArray<nsTArray<uint8_t>>>& stapledOCSPResponses,
    Maybe<nsTArray<uint8_t>>& sctsFromTLSExtension, uint32_t providerFlags) {
  UniqueCERTCertificate cert;
  UniqueCERTCertList certChain(CERT_NewCertList());
  if (!certChain) {
    return SECFailure;
  }
  if (MakeCertChain(peerCertChain, cert, certChain) != SECSuccess) {
    return SECFailure;
  }

  // we currently only support single stapled responses
  Maybe<nsTArray<uint8_t>> stapledOCSPResponse;
  if (stapledOCSPResponses && (stapledOCSPResponses->Length() == 1)) {
    stapledOCSPResponse.emplace(stapledOCSPResponses->ElementAt(0));
  }

  uint32_t certVerifierFlags = 0;
  // QuicTransportSecInfo does not have a SharedState as nsNSSSocketInfo.
  // Here we need prefs for ocsp. This are prefs they are the same for
  // PublicSSLState and PrivateSSLState, just take them from one of them.
  if (!PublicSSLState()->IsOCSPStaplingEnabled() ||
      !PublicSSLState()->IsOCSPMustStapleEnabled()) {
    certVerifierFlags |= CertVerifier::FLAG_TLS_IGNORE_STATUS_REQUEST;
  }

  return AuthCertificateHookInternal(
      infoObject, aPtrForLogging, cert, certChain, stapledOCSPResponse,
      sctsFromTLSExtension, providerFlags, certVerifierFlags);
}

SSLServerCertVerificationResult::SSLServerCertVerificationResult(
    TransportSecurityInfo* infoObject, PRErrorCode errorCode)
    : Runnable("psm::SSLServerCertVerificationResult"),
      mInfoObject(infoObject),
      mErrorCode(errorCode) {}

void SSLServerCertVerificationResult::Dispatch() {
  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsTarget =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(stsTarget, "Failed to get socket transport service event target");
  rv = stsTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "Failed to dispatch SSLServerCertVerificationResult");
}

NS_IMETHODIMP
SSLServerCertVerificationResult::Run() {
  // TODO: Assert that we're on the socket transport thread
  mInfoObject->SetCertVerificationResult(mErrorCode);
  return NS_OK;
}

}  // namespace psm
}  // namespace mozilla

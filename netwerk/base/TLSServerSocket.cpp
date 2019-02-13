/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TLSServerSocket.h"

#include "mozilla/net/DNS.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIServerSocket.h"
#include "nsITimer.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsNetCID.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsSocketTransport2.h"
#include "nsThreadUtils.h"
#include "ScopedNSSTypes.h"
#include "ssl.h"

extern PRThread *gSocketThread;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// TLSServerSocket
//-----------------------------------------------------------------------------

TLSServerSocket::TLSServerSocket()
  : mServerCert(nullptr)
{
}

TLSServerSocket::~TLSServerSocket()
{
}

NS_IMPL_ISUPPORTS_INHERITED(TLSServerSocket,
                            nsServerSocket,
                            nsITLSServerSocket)

nsresult
TLSServerSocket::SetSocketDefaults()
{
  // Set TLS options on the listening socket
  mFD = SSL_ImportFD(nullptr, mFD);
  if (NS_WARN_IF(!mFD)) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }

  SSL_OptionSet(mFD, SSL_SECURITY, true);
  SSL_OptionSet(mFD, SSL_HANDSHAKE_AS_CLIENT, false);
  SSL_OptionSet(mFD, SSL_HANDSHAKE_AS_SERVER, true);

  // We don't currently notify the server API consumer of renegotiation events
  // (to revalidate peer certs, etc.), so disable it for now.
  SSL_OptionSet(mFD, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_NEVER);

  SetSessionCache(true);
  SetSessionTickets(true);
  SetRequestClientCertificate(REQUEST_NEVER);

  return NS_OK;
}

void
TLSServerSocket::CreateClientTransport(PRFileDesc* aClientFD,
                                       const NetAddr& aClientAddr)
{
  MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
  nsresult rv;

  nsRefPtr<nsSocketTransport> trans = new nsSocketTransport;
  if (NS_WARN_IF(!trans)) {
    mCondition = NS_ERROR_OUT_OF_MEMORY;
    return;
  }

  nsRefPtr<TLSServerConnectionInfo> info = new TLSServerConnectionInfo();
  info->mServerSocket = this;
  info->mTransport = trans;
  nsCOMPtr<nsISupports> infoSupports =
    NS_ISUPPORTS_CAST(nsITLSServerConnectionInfo*, info);
  rv = trans->InitWithConnectedSocket(aClientFD, &aClientAddr, infoSupports);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mCondition = rv;
    return;
  }

  // Override the default peer certificate validation, so that server consumers
  // can make their own choice after the handshake completes.
  SSL_AuthCertificateHook(aClientFD, AuthCertificateHook, nullptr);
  // Once the TLS handshake has completed, the server consumer is notified and
  // has access to various TLS state details.
  // It's safe to pass info here because the socket transport holds it as
  // |mSecInfo| which keeps it alive for the lifetime of the socket.
  SSL_HandshakeCallback(aClientFD, TLSServerConnectionInfo::HandshakeCallback,
                        info);

  // Notify the consumer of the new client so it can manage the streams.
  // Security details aren't known yet.  The security observer will be notified
  // later when they are ready.
  nsCOMPtr<nsIServerSocket> serverSocket =
    do_QueryInterface(NS_ISUPPORTS_CAST(nsITLSServerSocket*, this));
  mListener->OnSocketAccepted(serverSocket, trans);
}

nsresult
TLSServerSocket::OnSocketListen()
{
  if (NS_WARN_IF(!mServerCert)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  ScopedCERTCertificate cert(mServerCert->GetCert());
  if (NS_WARN_IF(!cert)) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }

  ScopedSECKEYPrivateKey key(PK11_FindKeyByAnyCert(cert, nullptr));
  if (NS_WARN_IF(!key)) {
    return mozilla::psm::GetXPCOMFromNSSError(PR_GetError());
  }

  SSLKEAType certKEA = NSS_FindCertKEAType(cert);

  nsresult rv = MapSECStatus(SSL_ConfigSecureServer(mFD, cert, key, certKEA));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
SECStatus
TLSServerSocket::AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checksig,
                                     PRBool isServer)
{
  // Allow any client cert here, server consumer code can decide whether it's
  // okay after being notified of the new client socket.
  return SECSuccess;
}

//-----------------------------------------------------------------------------
// TLSServerSocket::nsITLSServerSocket
//-----------------------------------------------------------------------------

NS_IMETHODIMP
TLSServerSocket::GetServerCert(nsIX509Cert** aCert)
{
  if (NS_WARN_IF(!aCert)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aCert = mServerCert;
  NS_IF_ADDREF(*aCert);
  return NS_OK;
}

NS_IMETHODIMP
TLSServerSocket::SetServerCert(nsIX509Cert* aCert)
{
  // If AsyncListen was already called (and set mListener), it's too late to set
  // this.
  if (NS_WARN_IF(mListener)) {
    return NS_ERROR_IN_PROGRESS;
  }
  mServerCert = aCert;
  return NS_OK;
}

NS_IMETHODIMP
TLSServerSocket::SetSessionCache(bool aEnabled)
{
  // If AsyncListen was already called (and set mListener), it's too late to set
  // this.
  if (NS_WARN_IF(mListener)) {
    return NS_ERROR_IN_PROGRESS;
  }
  SSL_OptionSet(mFD, SSL_NO_CACHE, !aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TLSServerSocket::SetSessionTickets(bool aEnabled)
{
  // If AsyncListen was already called (and set mListener), it's too late to set
  // this.
  if (NS_WARN_IF(mListener)) {
    return NS_ERROR_IN_PROGRESS;
  }
  SSL_OptionSet(mFD, SSL_ENABLE_SESSION_TICKETS, aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TLSServerSocket::SetRequestClientCertificate(uint32_t aMode)
{
  // If AsyncListen was already called (and set mListener), it's too late to set
  // this.
  if (NS_WARN_IF(mListener)) {
    return NS_ERROR_IN_PROGRESS;
  }
  SSL_OptionSet(mFD, SSL_REQUEST_CERTIFICATE, aMode != REQUEST_NEVER);

  switch (aMode) {
    case REQUEST_ALWAYS:
      SSL_OptionSet(mFD, SSL_REQUIRE_CERTIFICATE, SSL_REQUIRE_NO_ERROR);
      break;
    case REQUIRE_FIRST_HANDSHAKE:
      SSL_OptionSet(mFD, SSL_REQUIRE_CERTIFICATE, SSL_REQUIRE_FIRST_HANDSHAKE);
      break;
    case REQUIRE_ALWAYS:
      SSL_OptionSet(mFD, SSL_REQUIRE_CERTIFICATE, SSL_REQUIRE_ALWAYS);
      break;
    default:
      SSL_OptionSet(mFD, SSL_REQUIRE_CERTIFICATE, SSL_REQUIRE_NEVER);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// TLSServerConnectionInfo
//-----------------------------------------------------------------------------

namespace {

class TLSServerSecurityObserverProxy final : public nsITLSServerSecurityObserver
{
  ~TLSServerSecurityObserverProxy() {}

public:
  explicit TLSServerSecurityObserverProxy(nsITLSServerSecurityObserver* aListener)
    : mListener(new nsMainThreadPtrHolder<nsITLSServerSecurityObserver>(aListener))
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITLSSERVERSECURITYOBSERVER

  class OnHandshakeDoneRunnable : public nsRunnable
  {
  public:
    OnHandshakeDoneRunnable(const nsMainThreadPtrHandle<nsITLSServerSecurityObserver>& aListener,
                            nsITLSServerSocket* aServer,
                            nsITLSClientStatus* aStatus)
      : mListener(aListener)
      , mServer(aServer)
      , mStatus(aStatus)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsITLSServerSecurityObserver> mListener;
    nsCOMPtr<nsITLSServerSocket> mServer;
    nsCOMPtr<nsITLSClientStatus> mStatus;
  };

private:
  nsMainThreadPtrHandle<nsITLSServerSecurityObserver> mListener;
};

NS_IMPL_ISUPPORTS(TLSServerSecurityObserverProxy,
                  nsITLSServerSecurityObserver)

NS_IMETHODIMP
TLSServerSecurityObserverProxy::OnHandshakeDone(nsITLSServerSocket* aServer,
                                                nsITLSClientStatus* aStatus)
{
  nsRefPtr<OnHandshakeDoneRunnable> r =
    new OnHandshakeDoneRunnable(mListener, aServer, aStatus);
  return NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
TLSServerSecurityObserverProxy::OnHandshakeDoneRunnable::Run()
{
  mListener->OnHandshakeDone(mServer, mStatus);
  return NS_OK;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS(TLSServerConnectionInfo,
                  nsITLSServerConnectionInfo,
                  nsITLSClientStatus)

TLSServerConnectionInfo::TLSServerConnectionInfo()
  : mServerSocket(nullptr)
  , mTransport(nullptr)
  , mPeerCert(nullptr)
  , mTlsVersionUsed(TLS_VERSION_UNKNOWN)
  , mKeyLength(0)
  , mMacLength(0)
  , mLock("TLSServerConnectionInfo.mLock")
  , mSecurityObserver(nullptr)
{
}

TLSServerConnectionInfo::~TLSServerConnectionInfo()
{
  if (!mSecurityObserver) {
    return;
  }

  nsITLSServerSecurityObserver* observer;
  {
    MutexAutoLock lock(mLock);
    mSecurityObserver.forget(&observer);
  }

  if (observer) {
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    NS_ProxyRelease(mainThread, observer);
  }
}

NS_IMETHODIMP
TLSServerConnectionInfo::SetSecurityObserver(nsITLSServerSecurityObserver* aObserver)
{
  {
    MutexAutoLock lock(mLock);
    mSecurityObserver = new TLSServerSecurityObserverProxy(aObserver);
  }
  return NS_OK;
}

NS_IMETHODIMP
TLSServerConnectionInfo::GetServerSocket(nsITLSServerSocket** aSocket)
{
  if (NS_WARN_IF(!aSocket)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aSocket = mServerSocket;
  NS_IF_ADDREF(*aSocket);
  return NS_OK;
}

NS_IMETHODIMP
TLSServerConnectionInfo::GetStatus(nsITLSClientStatus** aStatus)
{
  if (NS_WARN_IF(!aStatus)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aStatus = this;
  NS_IF_ADDREF(*aStatus);
  return NS_OK;
}

NS_IMETHODIMP
TLSServerConnectionInfo::GetPeerCert(nsIX509Cert** aCert)
{
  if (NS_WARN_IF(!aCert)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aCert = mPeerCert;
  NS_IF_ADDREF(*aCert);
  return NS_OK;
}

NS_IMETHODIMP
TLSServerConnectionInfo::GetTlsVersionUsed(int16_t* aTlsVersionUsed)
{
  if (NS_WARN_IF(!aTlsVersionUsed)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aTlsVersionUsed = mTlsVersionUsed;
  return NS_OK;
}

NS_IMETHODIMP
TLSServerConnectionInfo::GetCipherName(nsACString& aCipherName)
{
  aCipherName.Assign(mCipherName);
  return NS_OK;
}

NS_IMETHODIMP
TLSServerConnectionInfo::GetKeyLength(uint32_t* aKeyLength)
{
  if (NS_WARN_IF(!aKeyLength)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aKeyLength = mKeyLength;
  return NS_OK;
}

NS_IMETHODIMP
TLSServerConnectionInfo::GetMacLength(uint32_t* aMacLength)
{
  if (NS_WARN_IF(!aMacLength)) {
    return NS_ERROR_INVALID_POINTER;
  }
  *aMacLength = mMacLength;
  return NS_OK;
}

// static
void
TLSServerConnectionInfo::HandshakeCallback(PRFileDesc* aFD, void* aArg)
{
  nsRefPtr<TLSServerConnectionInfo> info =
    static_cast<TLSServerConnectionInfo*>(aArg);
  nsISocketTransport* transport = info->mTransport;
  // No longer needed outside this function, so clear the weak ref
  info->mTransport = nullptr;
  nsresult rv = info->HandshakeCallback(aFD);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    transport->Close(rv);
  }
}

nsresult
TLSServerConnectionInfo::HandshakeCallback(PRFileDesc* aFD)
{
  nsresult rv;

  ScopedCERTCertificate clientCert(SSL_PeerCertificate(aFD));
  if (clientCert) {
    nsCOMPtr<nsIX509CertDB> certDB =
      do_GetService(NS_X509CERTDB_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIX509Cert> clientCertPSM;
    rv = certDB->ConstructX509(reinterpret_cast<char*>(clientCert->derCert.data),
                               clientCert->derCert.len,
                               getter_AddRefs(clientCertPSM));
    if (NS_FAILED(rv)) {
      return rv;
    }

    mPeerCert = clientCertPSM;
  }

  SSLChannelInfo channelInfo;
  rv = MapSECStatus(SSL_GetChannelInfo(aFD, &channelInfo, sizeof(channelInfo)));
  if (NS_FAILED(rv)) {
    return rv;
  }
  mTlsVersionUsed = channelInfo.protocolVersion;

  SSLCipherSuiteInfo cipherInfo;
  rv = MapSECStatus(SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                                           sizeof(cipherInfo)));
  if (NS_FAILED(rv)) {
    return rv;
  }
  mCipherName.Assign(cipherInfo.cipherSuiteName);
  mKeyLength = cipherInfo.effectiveKeyBits;
  mMacLength = cipherInfo.macBits;

  if (!mSecurityObserver) {
    return NS_OK;
  }

  // Notify consumer code that handshake is complete
  nsCOMPtr<nsITLSServerSecurityObserver> observer;
  {
    MutexAutoLock lock(mLock);
    mSecurityObserver.swap(observer);
  }
  nsCOMPtr<nsITLSServerSocket> serverSocket;
  GetServerSocket(getter_AddRefs(serverSocket));
  observer->OnHandshakeDone(serverSocket, this);

  return NS_OK;
}

} // namespace net
} // namespace mozilla

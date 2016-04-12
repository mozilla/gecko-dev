/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_agent.h"

#include "pk11func.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include "keyhi.h"
#include "databuffer.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

namespace nss_test {


const char* TlsAgent::states[] = {"INIT", "CONNECTING", "CONNECTED", "ERROR"};

TlsAgent::TlsAgent(const std::string& name, Role role, Mode mode, SSLKEAType kea)
  : name_(name),
    mode_(mode),
    kea_(kea),
    server_key_bits_(0),
    pr_fd_(nullptr),
    adapter_(nullptr),
    ssl_fd_(nullptr),
    role_(role),
    state_(STATE_INIT),
    timer_handle_(nullptr),
    falsestart_enabled_(false),
    expected_version_(0),
    expected_cipher_suite_(0),
    expect_resumption_(false),
    can_falsestart_hook_called_(false),
    sni_hook_called_(false),
    auth_certificate_hook_called_(false),
    handshake_callback_called_(false),
    error_code_(0),
    send_ctr_(0),
    recv_ctr_(0),
    expected_read_error_(false),
    handshake_callback_(),
    auth_certificate_callback_() {

  memset(&info_, 0, sizeof(info_));
  memset(&csinfo_, 0, sizeof(csinfo_));
  SECStatus rv = SSL_VersionRangeGetDefault(mode_ == STREAM ?
                                            ssl_variant_stream : ssl_variant_datagram,
                                            &vrange_);
  EXPECT_EQ(SECSuccess, rv);
}

TlsAgent::~TlsAgent() {
  if (adapter_) {
    Poller::Instance()->Cancel(READABLE_EVENT, adapter_);
  }
  if (timer_handle_) {
    timer_handle_->Cancel();
  }

  if (pr_fd_) {
    PR_Close(pr_fd_);
  }

  if (ssl_fd_) {
    PR_Close(ssl_fd_);
  }
}

bool TlsAgent::EnsureTlsSetup() {
  // Don't set up twice
  if (ssl_fd_) return true;

  if (adapter_->mode() == STREAM) {
    ssl_fd_ = SSL_ImportFD(nullptr, pr_fd_);
  } else {
    ssl_fd_ = DTLS_ImportFD(nullptr, pr_fd_);
  }

  EXPECT_NE(nullptr, ssl_fd_);
  if (!ssl_fd_) return false;
  pr_fd_ = nullptr;

  if (role_ == SERVER) {
    CERTCertificate* cert = PK11_FindCertFromNickname(name_.c_str(), nullptr);
    EXPECT_NE(nullptr, cert);
    if (!cert) return false;

    SECKEYPublicKey* pub = CERT_ExtractPublicKey(cert);
    EXPECT_NE(nullptr, pub);
    if (!pub) return false;  // Leak cert.
    server_key_bits_ = SECKEY_PublicKeyStrengthInBits(pub);
    SECKEY_DestroyPublicKey(pub);

    SECKEYPrivateKey* priv = PK11_FindKeyByAnyCert(cert, nullptr);
    EXPECT_NE(nullptr, priv);
    if (!priv) return false;  // Leak cert.

    SECStatus rv = SSL_ConfigSecureServer(ssl_fd_, cert, priv, kea_);
    EXPECT_EQ(SECSuccess, rv);
    if (rv != SECSuccess) return false;  // Leak cert and key.

    SECKEY_DestroyPrivateKey(priv);
    CERT_DestroyCertificate(cert);

    rv = SSL_SNISocketConfigHook(ssl_fd_, SniHook, this);
    EXPECT_EQ(SECSuccess, rv);  // don't abort, just fail
  } else {
    SECStatus rv = SSL_SetURL(ssl_fd_, "server");
    EXPECT_EQ(SECSuccess, rv);
    if (rv != SECSuccess) return false;
  }

  SECStatus rv = SSL_VersionRangeSet(ssl_fd_, &vrange_);
  EXPECT_EQ(SECSuccess, rv);
  if (rv != SECSuccess) return false;

  rv = SSL_AuthCertificateHook(ssl_fd_, AuthCertificateHook, this);
  EXPECT_EQ(SECSuccess, rv);
  if (rv != SECSuccess) return false;

  rv = SSL_HandshakeCallback(ssl_fd_, HandshakeCallback, this);
  EXPECT_EQ(SECSuccess, rv);
  if (rv != SECSuccess) return false;

  return true;
}

void TlsAgent::SetupClientAuth() {
  EXPECT_TRUE(EnsureTlsSetup());
  ASSERT_EQ(CLIENT, role_);

  EXPECT_EQ(SECSuccess,
            SSL_GetClientAuthDataHook(ssl_fd_, GetClientAuthDataHook,
                                      reinterpret_cast<void*>(this)));
}

bool TlsAgent::GetClientAuthCredentials(CERTCertificate **cert,
                                        SECKEYPrivateKey **priv) const {
  *cert = PK11_FindCertFromNickname(name_.c_str(), nullptr);
  EXPECT_NE(nullptr, *cert);
  if (!*cert) return false;

  *priv = PK11_FindKeyByAnyCert(*cert, nullptr);
  EXPECT_NE(nullptr, *priv);
  if (!*priv) return false; // Leak cert.

  return true;
}

SECStatus TlsAgent::GetClientAuthDataHook(void* self, PRFileDesc* fd,
                                          CERTDistNames* caNames,
                                          CERTCertificate** cert,
                                          SECKEYPrivateKey** privKey) {
  TlsAgent* agent = reinterpret_cast<TlsAgent*>(self);
  if (agent->GetClientAuthCredentials(cert, privKey)) {
    return SECSuccess;
  }
  return SECFailure;
}


void TlsAgent::RequestClientAuth(bool requireAuth) {
  EXPECT_TRUE(EnsureTlsSetup());
  ASSERT_EQ(SERVER, role_);

  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(ssl_fd_, SSL_REQUEST_CERTIFICATE, PR_TRUE));
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(ssl_fd_, SSL_REQUIRE_CERTIFICATE,
                          requireAuth ? PR_TRUE : PR_FALSE));

  EXPECT_EQ(SECSuccess,
            SSL_AuthCertificateHook(ssl_fd_, &TlsAgent::ClientAuthenticated,
                                    this));
  expect_client_auth_ = true;
}

void TlsAgent::StartConnect() {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv;
  rv = SSL_ResetHandshake(ssl_fd_, role_ == SERVER ? PR_TRUE : PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
  SetState(STATE_CONNECTING);
}

void TlsAgent::DisableCiphersByKeyExchange(SSLKEAType kea) {
  EXPECT_TRUE(EnsureTlsSetup());

  for (size_t i = 0; i < SSL_NumImplementedCiphers; ++i) {
    SSLCipherSuiteInfo csinfo;

    SECStatus rv = SSL_GetCipherSuiteInfo(SSL_ImplementedCiphers[i],
                                          &csinfo, sizeof(csinfo));
    ASSERT_EQ(SECSuccess, rv);
    EXPECT_EQ(sizeof(csinfo), csinfo.length);

    if (csinfo.keaType == kea) {
      rv = SSL_CipherPrefSet(ssl_fd_, SSL_ImplementedCiphers[i], PR_FALSE);
      EXPECT_EQ(SECSuccess, rv);
    }
  }
}

void TlsAgent::SetSessionTicketsEnabled(bool en) {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_ENABLE_SESSION_TICKETS,
                               en ? PR_TRUE : PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
}

void TlsAgent::SetSessionCacheEnabled(bool en) {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_NO_CACHE,
                               en ? PR_FALSE : PR_TRUE);
  EXPECT_EQ(SECSuccess, rv);
}

void TlsAgent::SetVersionRange(uint16_t minver, uint16_t maxver) {
   vrange_.min = minver;
   vrange_.max = maxver;

   if (ssl_fd_) {
     SECStatus rv = SSL_VersionRangeSet(ssl_fd_, &vrange_);
     EXPECT_EQ(SECSuccess, rv);
   }
}

void TlsAgent::GetVersionRange(uint16_t* minver, uint16_t* maxver) {
  *minver = vrange_.min;
  *maxver = vrange_.max;
}

void TlsAgent::SetExpectedVersion(uint16_t version) {
  expected_version_ = version;
}

void TlsAgent::SetServerKeyBits(uint16_t bits) {
  server_key_bits_ = bits;
}

void TlsAgent::SetExpectedReadError(bool err) {
  expected_read_error_ = err;
}

void TlsAgent::SetSignatureAlgorithms(const SSLSignatureAndHashAlg* algorithms,
                                      size_t count) {
  EXPECT_TRUE(EnsureTlsSetup());
  EXPECT_LE(count, SSL_SignatureMaxCount());
  EXPECT_EQ(SECSuccess, SSL_SignaturePrefSet(ssl_fd_, algorithms,
                                             static_cast<unsigned int>(count)));
  EXPECT_EQ(SECFailure, SSL_SignaturePrefSet(ssl_fd_, algorithms, 0))
      << "setting no algorithms should fail and do nothing";

  std::vector<SSLSignatureAndHashAlg> configuredAlgorithms(count);
  unsigned int configuredCount;
  EXPECT_EQ(SECFailure,
            SSL_SignaturePrefGet(ssl_fd_, nullptr, &configuredCount, 1))
      << "get algorithms, algorithms is nullptr";
  EXPECT_EQ(SECFailure,
            SSL_SignaturePrefGet(ssl_fd_, &configuredAlgorithms[0],
                                 &configuredCount, 0))
      << "get algorithms, too little space";
  EXPECT_EQ(SECFailure,
            SSL_SignaturePrefGet(ssl_fd_, &configuredAlgorithms[0],
                                 nullptr, configuredAlgorithms.size()))
      << "get algorithms, algCountOut is nullptr";

  EXPECT_EQ(SECSuccess,
            SSL_SignaturePrefGet(ssl_fd_, &configuredAlgorithms[0],
                                 &configuredCount,
                                 configuredAlgorithms.size()));
  // SignaturePrefSet drops unsupported algorithms silently, so the number that
  // are configured might be fewer.
  EXPECT_LE(configuredCount, count);
  unsigned int i = 0;
  for (unsigned int j = 0; j < count && i < configuredCount; ++j) {
    if (i < configuredCount &&
        algorithms[j].hashAlg == configuredAlgorithms[i].hashAlg &&
        algorithms[j].sigAlg == configuredAlgorithms[i].sigAlg) {
      ++i;
    }
  }
  EXPECT_EQ(i, configuredCount) << "algorithms in use were all set";
}

void TlsAgent::CheckKEAType(SSLKEAType type) const {
  EXPECT_EQ(STATE_CONNECTED, state_);
  EXPECT_EQ(type, csinfo_.keaType);

  PRUint32 ecKEAKeyBits = SSLInt_DetermineKEABits(server_key_bits_,
                                                  csinfo_.authAlgorithm);

  switch (type) {
      case ssl_kea_ecdh:
          EXPECT_EQ(ecKEAKeyBits, info_.keaKeyBits);
          break;
      case ssl_kea_dh:
          EXPECT_EQ(2048U, info_.keaKeyBits);
          break;
      case ssl_kea_rsa:
          EXPECT_EQ(server_key_bits_, info_.keaKeyBits);
          break;
      default:
          break;
  }
}

void TlsAgent::CheckAuthType(SSLAuthType type) const {
  EXPECT_EQ(STATE_CONNECTED, state_);
  EXPECT_EQ(type, csinfo_.authAlgorithm);
  EXPECT_EQ(server_key_bits_, info_.authKeyBits);
  switch (type) {
      case ssl_auth_ecdsa:
          // extra check for P-256
          EXPECT_EQ(256U, info_.authKeyBits);
          break;
      default:
          break;
  }
}

void TlsAgent::EnableFalseStart() {
  EXPECT_TRUE(EnsureTlsSetup());

  falsestart_enabled_ = true;
  EXPECT_EQ(SECSuccess,
            SSL_SetCanFalseStartCallback(ssl_fd_, CanFalseStartCallback, this));
  EXPECT_EQ(SECSuccess,
            SSL_OptionSet(ssl_fd_, SSL_ENABLE_FALSE_START, PR_TRUE));
}

void TlsAgent::ExpectResumption() {
  expect_resumption_ = true;
}

void TlsAgent::EnableAlpn(const uint8_t* val, size_t len) {
  EXPECT_TRUE(EnsureTlsSetup());

  EXPECT_EQ(SECSuccess, SSL_OptionSet(ssl_fd_, SSL_ENABLE_ALPN, PR_TRUE));
  EXPECT_EQ(SECSuccess, SSL_SetNextProtoNego(ssl_fd_, val, len));
}

void TlsAgent::CheckAlpn(SSLNextProtoState expected_state,
                         const std::string& expected) const {
  SSLNextProtoState state;
  char chosen[10];
  unsigned int chosen_len;
  SECStatus rv = SSL_GetNextProto(ssl_fd_, &state,
                                  reinterpret_cast<unsigned char*>(chosen),
                                  &chosen_len, sizeof(chosen));
  EXPECT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected_state, state);
  EXPECT_EQ(expected, std::string(chosen, chosen_len));
}

void TlsAgent::EnableSrtp() {
  EXPECT_TRUE(EnsureTlsSetup());
  const uint16_t ciphers[] = {
    SRTP_AES128_CM_HMAC_SHA1_80, SRTP_AES128_CM_HMAC_SHA1_32
  };
  EXPECT_EQ(SECSuccess, SSL_SetSRTPCiphers(ssl_fd_, ciphers,
                                           PR_ARRAY_SIZE(ciphers)));
}

void TlsAgent::CheckSrtp() const {
  uint16_t actual;
  EXPECT_EQ(SECSuccess, SSL_GetSRTPCipher(ssl_fd_, &actual));
  EXPECT_EQ(SRTP_AES128_CM_HMAC_SHA1_80, actual);
}

void TlsAgent::CheckErrorCode(int32_t expected) const {
  EXPECT_EQ(STATE_ERROR, state_);
  EXPECT_EQ(expected, error_code_);
}

void TlsAgent::CheckPreliminaryInfo() {
  SSLPreliminaryChannelInfo info;
  EXPECT_EQ(SECSuccess,
            SSL_GetPreliminaryChannelInfo(ssl_fd_, &info, sizeof(info)));
  EXPECT_EQ(sizeof(info), info.length);
  EXPECT_TRUE(info.valuesSet & ssl_preinfo_version);
  EXPECT_TRUE(info.valuesSet & ssl_preinfo_cipher_suite);

  // A version of 0 is invalid and indicates no expectation.  This value is
  // initialized to 0 so that tests that don't explicitly set an expected
  // version can negotiate a version.
  if (!expected_version_) {
    expected_version_ = info.protocolVersion;
  }
  EXPECT_EQ(expected_version_, info.protocolVersion);

  // As with the version; 0 is the null cipher suite (and also invalid).
  if (!expected_cipher_suite_) {
    expected_cipher_suite_ = info.cipherSuite;
  }
  EXPECT_EQ(expected_cipher_suite_, info.cipherSuite);
}

// Check that all the expected callbacks have been called.
void TlsAgent::CheckCallbacks() const {
  // If false start happens, the handshake is reported as being complete at the
  // point that false start happens.
  if (expect_resumption_ || !falsestart_enabled_) {
    EXPECT_TRUE(handshake_callback_called_);
  }

  // These callbacks shouldn't fire if we are resuming, except on TLS 1.3.
  if (role_ == SERVER) {
    PRBool have_sni = SSLInt_ExtensionNegotiated(ssl_fd_, ssl_server_name_xtn);
    EXPECT_EQ(((!expect_resumption_ && have_sni) ||
               expected_version_ >= SSL_LIBRARY_VERSION_TLS_1_3), sni_hook_called_);
  } else {
    EXPECT_EQ(!expect_resumption_, auth_certificate_hook_called_);
    // Note that this isn't unconditionally called, even with false start on.
    // But the callback is only skipped if a cipher that is ridiculously weak
    // (80 bits) is chosen.  Don't test that: plan to remove bad ciphers.
    EXPECT_EQ(falsestart_enabled_ && !expect_resumption_,
              can_falsestart_hook_called_);
  }
}

void TlsAgent::Connected() {
  LOG("Handshake success");
  CheckPreliminaryInfo();
  CheckCallbacks();

  SECStatus rv = SSL_GetChannelInfo(ssl_fd_, &info_, sizeof(info_));
  EXPECT_EQ(SECSuccess, rv);
  EXPECT_EQ(sizeof(info_), info_.length);

  // Preliminary values are exposed through callbacks during the handshake.
  // If either expected values were set or the callbacks were called, check
  // that the final values are correct.
  EXPECT_EQ(expected_version_, info_.protocolVersion);
  EXPECT_EQ(expected_cipher_suite_, info_.cipherSuite);

  rv = SSL_GetCipherSuiteInfo(info_.cipherSuite, &csinfo_, sizeof(csinfo_));
  EXPECT_EQ(SECSuccess, rv);
  EXPECT_EQ(sizeof(csinfo_), csinfo_.length);

  SetState(STATE_CONNECTED);
}

void TlsAgent::EnableExtendedMasterSecret() {
  ASSERT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_,
                               SSL_ENABLE_EXTENDED_MASTER_SECRET,
                               PR_TRUE);

  ASSERT_EQ(SECSuccess, rv);
}

void TlsAgent::CheckExtendedMasterSecret(bool expected) {
  if (version() >= SSL_LIBRARY_VERSION_TLS_1_3) {
    expected = PR_TRUE;
  }
  ASSERT_EQ(expected, info_.extendedMasterSecretUsed != PR_FALSE)
      << "unexpected extended master secret state for " << name_;
}

void TlsAgent::DisableRollbackDetection() {
  ASSERT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_,
                               SSL_ROLLBACK_DETECTION,
                               PR_FALSE);

  ASSERT_EQ(SECSuccess, rv);
}

void TlsAgent::EnableCompression() {
  ASSERT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_ENABLE_DEFLATE, PR_TRUE);
  ASSERT_EQ(SECSuccess, rv);
}

void TlsAgent::SetDowngradeCheckVersion(uint16_t version) {
  ASSERT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_SetDowngradeCheckVersion(ssl_fd_, version);
  ASSERT_EQ(SECSuccess, rv);
}

void TlsAgent::Handshake() {
  LOG("Handshake");
  SECStatus rv = SSL_ForceHandshake(ssl_fd_);
  if (rv == SECSuccess) {
    Connected();

    Poller::Instance()->Wait(READABLE_EVENT, adapter_, this,
                             &TlsAgent::ReadableCallback);
    return;
  }

  int32_t err = PR_GetError();
  switch (err) {
    case PR_WOULD_BLOCK_ERROR:
      LOG("Would have blocked");
      if (mode_ == DGRAM) {
        if (timer_handle_) {
          timer_handle_->Cancel();
        }

        PRIntervalTime timeout;
        rv = DTLS_GetHandshakeTimeout(ssl_fd_, &timeout);
        if (rv == SECSuccess) {
          Poller::Instance()->SetTimer(timeout, this,
                                       &TlsAgent::ReadableCallback,
                                       &timer_handle_);
        }
      }
      Poller::Instance()->Wait(READABLE_EVENT, adapter_, this,
                               &TlsAgent::ReadableCallback);
      return;

    case SSL_ERROR_RX_MALFORMED_HANDSHAKE:
    default:
      if (IS_SSL_ERROR(err)) {
        LOG("Handshake failed with SSL error " << (err - SSL_ERROR_BASE)
            << ": " << PORT_ErrorToString(err));
      } else {
        LOG("Handshake failed with error " << err
            << ": " << PORT_ErrorToString(err));
      }
      error_code_ = err;
      SetState(STATE_ERROR);
      return;
  }
}

void TlsAgent::PrepareForRenegotiate() {
  EXPECT_EQ(STATE_CONNECTED, state_);

  SetState(STATE_CONNECTING);
}

void TlsAgent::StartRenegotiate() {
  PrepareForRenegotiate();

  SECStatus rv = SSL_ReHandshake(ssl_fd_, PR_TRUE);
  EXPECT_EQ(SECSuccess, rv);
}

void TlsAgent::SendDirect(const DataBuffer& buf) {
  LOG("Send Direct " << buf);
  adapter_->peer()->PacketReceived(buf);
}

void TlsAgent::SendData(size_t bytes, size_t blocksize) {
  uint8_t block[4096];

  ASSERT_LT(blocksize, sizeof(block));

  while(bytes) {
    size_t tosend = std::min(blocksize, bytes);

    for(size_t i = 0; i < tosend; ++i) {
      block[i] = 0xff & send_ctr_;
      ++send_ctr_;
    }

    LOG("Writing " << tosend << " bytes");
    int32_t rv = PR_Write(ssl_fd_, block, tosend);
    ASSERT_EQ(tosend, static_cast<size_t>(rv));

    bytes -= tosend;
  }
}

void TlsAgent::ReadBytes() {
  uint8_t block[1024];

  int32_t rv = PR_Read(ssl_fd_, block, sizeof(block));
  LOG("ReadBytes " << rv);

  if (rv >= 0) {
    size_t count = static_cast<size_t>(rv);
    for (size_t i = 0; i < count; ++i) {
      ASSERT_EQ(recv_ctr_ & 0xff, block[i]);
      recv_ctr_++;
    }
  } else {
    int32_t err = PR_GetError();
    LOG("Read error " << err << ": " << PORT_ErrorToString(err));
    if (err != PR_WOULD_BLOCK_ERROR && expected_read_error_) {
      error_code_ = err;
    }
  }

  // If closed, then don't bother waiting around.
  if (rv) {
    Poller::Instance()->Wait(READABLE_EVENT, adapter_, this,
                             &TlsAgent::ReadableCallback);
  }
}

void TlsAgent::ResetSentBytes() {
  send_ctr_ = 0;
}

void TlsAgent::ConfigureSessionCache(SessionResumptionMode mode) {
  EXPECT_TRUE(EnsureTlsSetup());

  SECStatus rv = SSL_OptionSet(ssl_fd_,
                               SSL_NO_CACHE,
                               mode & RESUME_SESSIONID ?
                               PR_FALSE : PR_TRUE);
  EXPECT_EQ(SECSuccess, rv);

  rv = SSL_OptionSet(ssl_fd_,
                     SSL_ENABLE_SESSION_TICKETS,
                     mode & RESUME_TICKET ?
                     PR_TRUE : PR_FALSE);
  EXPECT_EQ(SECSuccess, rv);
}

static const std::string kTlsRolesAllArr[] = {"CLIENT", "SERVER"};
::testing::internal::ParamGenerator<std::string>
  TlsAgentTestBase::kTlsRolesAll = ::testing::ValuesIn(kTlsRolesAllArr);

void TlsAgentTestBase::Init() {
  agent_ = new TlsAgent(
      role_ == TlsAgent::CLIENT ? "client" : "server",
      role_, mode_, kea_);
  agent_->Init();
  fd_ = DummyPrSocket::CreateFD("dummy", mode_);
  agent_->adapter()->SetPeer(
      DummyPrSocket::GetAdapter(fd_));
  agent_->StartConnect();
}

void TlsAgentTestBase::EnsureInit() {
  if (!agent_) {
    Init();
  }
}

void TlsAgentTestBase::ProcessMessage(const DataBuffer& buffer,
                                      TlsAgent::State expected_state,
                                      int32_t error_code) {
  EnsureInit();
  agent_->adapter()->PacketReceived(buffer);
  agent_->Handshake();

  ASSERT_EQ(expected_state, agent_->state());

  if (expected_state == TlsAgent::STATE_ERROR) {
    ASSERT_EQ(error_code, agent_->error_code());
  }
}

} // namespace nss_test

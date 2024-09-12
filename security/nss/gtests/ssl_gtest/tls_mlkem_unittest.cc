/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

extern "C" {
// This is not something that should make you happy.
#include "libssl_internals.h"
}

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

TEST_P(TlsKeyExchangeTest13, Mlkem768x25519Supported) {
  EnsureKeyShareSetup();
  ConfigNamedGroups({ssl_grp_kem_mlkem768x25519});

  Connect();
  CheckKeys(ssl_kea_ecdh_hybrid, ssl_grp_kem_mlkem768x25519, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);
}

TEST_P(TlsKeyExchangeTest, Tls12ClientMlkem768x25519NotSupported) {
  EnsureKeyShareSetup();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh_hybrid);
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(
                            client_->ssl_fd(),
                            kECDHEGroups.size() + kEcdhHybridGroups.size()));

  Connect();
  std::vector<SSLNamedGroup> groups = GetGroupDetails(groups_capture_);
  for (auto group : groups) {
    EXPECT_NE(group, ssl_grp_kem_mlkem768x25519);
  }
}

TEST_P(TlsKeyExchangeTest13, Tls12ServerMlkem768x25519NotSupported) {
  if (variant_ == ssl_variant_datagram) {
    /* Bug 1874451 - reenable this test */
    return;
  }

  EnsureKeyShareSetup();

  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_3);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);

  client_->DisableAllCiphers();
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  client_->EnableCiphersByKeyExchange(ssl_kea_ecdh_hybrid);
  client_->ConfigNamedGroups(
      {ssl_grp_kem_mlkem768x25519, ssl_grp_ec_curve25519});
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  server_->EnableCiphersByKeyExchange(ssl_kea_ecdh);
  server_->EnableCiphersByKeyExchange(ssl_kea_ecdh_hybrid);
  server_->ConfigNamedGroups(
      {ssl_grp_kem_mlkem768x25519, ssl_grp_ec_curve25519});

  Connect();
  CheckKeys(ssl_kea_ecdh, ssl_grp_ec_curve25519, ssl_auth_rsa_sign,
            ssl_sig_rsa_pss_rsae_sha256);
}

TEST_P(TlsKeyExchangeTest13, Mlkem768x25519ClientDisabledByPolicy) {
  EnsureKeyShareSetup();
  client_->SetPolicy(SEC_OID_MLKEM768X25519, 0, NSS_USE_ALG_IN_SSL_KX);
  ConfigNamedGroups({ssl_grp_kem_mlkem768x25519, ssl_grp_ec_secp256r1});

  Connect();
  CheckKEXDetails({ssl_grp_ec_secp256r1}, {ssl_grp_ec_secp256r1});
}

TEST_P(TlsKeyExchangeTest13, Mlkem768x25519ServerDisabledByPolicy) {
  EnsureKeyShareSetup();
  server_->SetPolicy(SEC_OID_MLKEM768X25519, 0, NSS_USE_ALG_IN_SSL_KX);
  ConfigNamedGroups({ssl_grp_kem_mlkem768x25519, ssl_grp_ec_secp256r1});

  Connect();
  CheckKEXDetails({ssl_grp_kem_mlkem768x25519, ssl_grp_ec_secp256r1},
                  {ssl_grp_kem_mlkem768x25519}, ssl_grp_ec_secp256r1);
}

static void CheckECDHShareReuse(
    const std::shared_ptr<TlsExtensionCapture>& capture) {
  EXPECT_TRUE(capture->captured());
  const DataBuffer& ext = capture->extension();
  DataBuffer hybrid_share;
  DataBuffer x25519_share;

  size_t offset = 0;
  uint32_t ext_len;
  ext.Read(0, 2, &ext_len);
  EXPECT_EQ(ext.len() - 2, ext_len);
  offset += 2;

  uint32_t named_group;
  uint32_t named_group_len;
  ext.Read(offset, 2, &named_group);
  ext.Read(offset + 2, 2, &named_group_len);
  while (offset < ext.len()) {
    if (named_group == ssl_grp_kem_mlkem768x25519) {
      hybrid_share = DataBuffer(ext.data() + offset + 2 + 2, named_group_len);
    }
    if (named_group == ssl_grp_ec_curve25519) {
      x25519_share = DataBuffer(ext.data() + offset + 2 + 2, named_group_len);
    }
    offset += 2 + 2 + named_group_len;
    ext.Read(offset, 2, &named_group);
    ext.Read(offset + 2, 2, &named_group_len);
  }
  EXPECT_EQ(offset, ext.len());

  ASSERT_TRUE(hybrid_share.data());
  ASSERT_TRUE(x25519_share.data());
  ASSERT_GT(hybrid_share.len(), x25519_share.len());
  EXPECT_EQ(0, memcmp(hybrid_share.data() + KYBER768_PUBLIC_KEY_BYTES,
                      x25519_share.data(), x25519_share.len()));
}

TEST_P(TlsKeyExchangeTest13, Mlkem768x25519ShareReuseFirst) {
  if (variant_ == ssl_variant_datagram) {
    /* Bug 1874451 - reenable this test */
    return;
  }
  EnsureKeyShareSetup();
  ConfigNamedGroups({ssl_grp_kem_mlkem768x25519, ssl_grp_ec_curve25519});
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  Connect();

  CheckKEXDetails({ssl_grp_kem_mlkem768x25519, ssl_grp_ec_curve25519},
                  {ssl_grp_kem_mlkem768x25519, ssl_grp_ec_curve25519});
  CheckECDHShareReuse(shares_capture_);
}

TEST_P(TlsKeyExchangeTest13, Mlkem768x25519ShareReuseSecond) {
  if (variant_ == ssl_variant_datagram) {
    /* Bug 1874451 - reenable this test */
    return;
  }
  EnsureKeyShareSetup();
  ConfigNamedGroups({ssl_grp_ec_curve25519, ssl_grp_kem_mlkem768x25519});
  EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));

  Connect();

  CheckKEXDetails({ssl_grp_ec_curve25519, ssl_grp_kem_mlkem768x25519},
                  {ssl_grp_ec_curve25519, ssl_grp_kem_mlkem768x25519});
  CheckECDHShareReuse(shares_capture_);
}

class Mlkem768x25519ShareDamager : public TlsExtensionFilter {
 public:
  typedef enum {
    downgrade,
    extend,
    truncate,
    zero_ecdh,
    modify_ecdh,
    modify_mlkem,
    modify_mlkem_pubkey_mod_q,
  } damage_type;

  Mlkem768x25519ShareDamager(const std::shared_ptr<TlsAgent>& a,
                             damage_type damage)
      : TlsExtensionFilter(a), damage_(damage) {}

  virtual PacketFilter::Action FilterExtension(uint16_t extension_type,
                                               const DataBuffer& input,
                                               DataBuffer* output) {
    if (extension_type != ssl_tls13_key_share_xtn) {
      return KEEP;
    }

    // Find the Mlkem768x25519 share
    size_t offset = 0;
    if (agent()->role() == TlsAgent::CLIENT) {
      offset += 2;  // skip KeyShareClientHello length
    }

    uint32_t named_group;
    uint32_t named_group_len;
    input.Read(offset, 2, &named_group);
    input.Read(offset + 2, 2, &named_group_len);
    while (named_group != ssl_grp_kem_mlkem768x25519) {
      offset += 2 + 2 + named_group_len;
      input.Read(offset, 2, &named_group);
      input.Read(offset + 2, 2, &named_group_len);
    }
    EXPECT_EQ(named_group, ssl_grp_kem_mlkem768x25519);

    DataBuffer hybrid_key_share(input.data() + offset, 2 + 2 + named_group_len);

    // Damage the Mlkem768x25519 share
    uint32_t mlkem_component_len =
        hybrid_key_share.len() - 2 - 2 - X25519_PUBLIC_KEY_BYTES;
    unsigned char* ecdh_component =
        hybrid_key_share.data() + 2 + 2 + mlkem_component_len;
    unsigned char* mlkem_component = hybrid_key_share.data() + 2 + 2;
    switch (damage_) {
      case Mlkem768x25519ShareDamager::downgrade:
        // Downgrade a Mlkem768x25519 share to X25519
        memcpy(mlkem_component, ecdh_component, X25519_PUBLIC_KEY_BYTES);
        hybrid_key_share.Truncate(2 + 2 + X25519_PUBLIC_KEY_BYTES);
        hybrid_key_share.Write(0, ssl_grp_ec_curve25519, 2);
        hybrid_key_share.Write(2, X25519_PUBLIC_KEY_BYTES, 2);
        break;
      case Mlkem768x25519ShareDamager::truncate:
        // Truncate a Mlkem768x25519 share before the X25519 component
        hybrid_key_share.Truncate(2 + 2 + mlkem_component_len);
        hybrid_key_share.Write(2, mlkem_component_len, 2);
        break;
      case Mlkem768x25519ShareDamager::extend:
        // Append 4 bytes to a Mlkem768x25519 share
        uint32_t current_len;
        hybrid_key_share.Read(2, 2, &current_len);
        hybrid_key_share.Write(hybrid_key_share.len(), current_len, 4);
        hybrid_key_share.Write(2, current_len + 4, 2);
        break;
      case Mlkem768x25519ShareDamager::zero_ecdh:
        // Replace an X25519 component with 0s
        memset(ecdh_component, 0, X25519_PUBLIC_KEY_BYTES);
        break;
      case Mlkem768x25519ShareDamager::modify_ecdh:
        // Flip a bit in the X25519 component
        ecdh_component[0] ^= 0x01;
        break;
      case Mlkem768x25519ShareDamager::modify_mlkem:
        // Flip a bit in the mlkem component
        mlkem_component[0] ^= 0x01;
        break;
      case Mlkem768x25519ShareDamager::modify_mlkem_pubkey_mod_q:
        if (agent()->role() == TlsAgent::CLIENT) {
          // Replace the client's public key with an sequence of 12-bit values
          // in the same equivalence class mod 3329. The FIPS-203 input
          // validation check should fail.
          for (size_t i = 0; i < mlkem_component_len - 32; i += 3) {
            // Pairs of 12-bit coefficients are packed into 3 bytes.
            // Unpack them, change equivalence class if possible, and repack.
            uint16_t coeff0 =
                mlkem_component[i] | ((mlkem_component[i + 1] & 0x0f) << 8);
            uint16_t coeff1 = (mlkem_component[i + 1] & 0xf0 >> 4) |
                              ((mlkem_component[i + 2]) << 4);
            if (coeff0 < 4096 - 3329) {
              coeff0 += 3329;
            }
            if (coeff1 < 4096 - 3329) {
              coeff1 += 3329;
            }
            mlkem_component[i] = coeff0;
            mlkem_component[i + 1] = (coeff0 >> 8) + ((coeff1 & 0x0f) << 4);
            mlkem_component[i + 2] = coeff1 >> 4;
          }
        }
        break;
    }

    *output = input;
    output->Splice(hybrid_key_share, offset, 2 + 2 + named_group_len);

    // Fix the KeyShareClientHello length if necessary
    if (agent()->role() == TlsAgent::CLIENT &&
        hybrid_key_share.len() != 2 + 2 + named_group_len) {
      output->Write(0, output->len() - 2, 2);
    }

    return CHANGE;
  }

 private:
  damage_type damage_;
};

class TlsMlkem768x25519DamageTest
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<
          Mlkem768x25519ShareDamager::damage_type> {
 public:
  TlsMlkem768x25519DamageTest()
      : TlsConnectTestBase(ssl_variant_stream, SSL_LIBRARY_VERSION_TLS_1_3) {}

 protected:
  void Damage(const std::shared_ptr<TlsAgent>& agent) {
    EnsureTlsSetup();
    client_->ConfigNamedGroups(
        {ssl_grp_ec_curve25519, ssl_grp_kem_mlkem768x25519});
    server_->ConfigNamedGroups(
        {ssl_grp_kem_mlkem768x25519, ssl_grp_ec_curve25519});
    EXPECT_EQ(SECSuccess, SSL_SendAdditionalKeyShares(client_->ssl_fd(), 1));
    MakeTlsFilter<Mlkem768x25519ShareDamager>(agent, GetParam());
  }
};

TEST_P(TlsMlkem768x25519DamageTest, DamageClientShare) {
  Damage(client_);

  switch (GetParam()) {
    case Mlkem768x25519ShareDamager::extend:
    case Mlkem768x25519ShareDamager::truncate:
      ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
      server_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_HYBRID_KEY_SHARE);
      break;
    case Mlkem768x25519ShareDamager::zero_ecdh:
      ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
      server_->CheckErrorCode(SEC_ERROR_INVALID_KEY);
      break;
    case Mlkem768x25519ShareDamager::modify_mlkem_pubkey_mod_q:
      ConnectExpectAlert(server_, kTlsAlertIllegalParameter);
      server_->CheckErrorCode(SEC_ERROR_INVALID_ARGS);
      break;
    case Mlkem768x25519ShareDamager::downgrade:
    case Mlkem768x25519ShareDamager::modify_ecdh:
    case Mlkem768x25519ShareDamager::modify_mlkem:
      client_->ExpectSendAlert(kTlsAlertBadRecordMac);
      server_->ExpectSendAlert(kTlsAlertBadRecordMac);
      ConnectExpectFail();
      client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      break;
  }
}

TEST_P(TlsMlkem768x25519DamageTest, DamageServerShare) {
  Damage(server_);

  switch (GetParam()) {
    case Mlkem768x25519ShareDamager::extend:
    case Mlkem768x25519ShareDamager::truncate:
      client_->ExpectSendAlert(kTlsAlertIllegalParameter);
      server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
      ConnectExpectFail();
      client_->CheckErrorCode(SSL_ERROR_RX_MALFORMED_HYBRID_KEY_SHARE);
      break;
    case Mlkem768x25519ShareDamager::zero_ecdh:
      client_->ExpectSendAlert(kTlsAlertIllegalParameter);
      server_->ExpectSendAlert(kTlsAlertUnexpectedMessage);
      ConnectExpectFail();
      client_->CheckErrorCode(SEC_ERROR_INVALID_KEY);
      break;
    case Mlkem768x25519ShareDamager::downgrade:
    case Mlkem768x25519ShareDamager::modify_ecdh:
    case Mlkem768x25519ShareDamager::modify_mlkem:
      client_->ExpectSendAlert(kTlsAlertBadRecordMac);
      server_->ExpectSendAlert(kTlsAlertBadRecordMac);
      ConnectExpectFail();
      client_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      server_->CheckErrorCode(SSL_ERROR_BAD_MAC_READ);
      break;
    case Mlkem768x25519ShareDamager::modify_mlkem_pubkey_mod_q:
      // The server doesn't send a public key, so nothing is changed.
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    TlsMlkem768x25519DamageTest, TlsMlkem768x25519DamageTest,
    ::testing::Values(Mlkem768x25519ShareDamager::downgrade,
                      Mlkem768x25519ShareDamager::extend,
                      Mlkem768x25519ShareDamager::truncate,
                      Mlkem768x25519ShareDamager::zero_ecdh,
                      Mlkem768x25519ShareDamager::modify_ecdh,
                      Mlkem768x25519ShareDamager::modify_mlkem,
                      Mlkem768x25519ShareDamager::modify_mlkem_pubkey_mod_q));

}  // namespace nss_test

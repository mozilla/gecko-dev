/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "blapi.h"
#include "shared.h"
#include "ssl.h"
#include "sslexp.h"
#include "sslimpl.h"
#include "sslt.h"
#include "tls_client_config.h"
#include "tls_common.h"
#include "tls_mutators.h"
#include "tls_socket.h"

#ifdef IS_DTLS_FUZZ
__attribute__((constructor)) static void set_is_dtls() {
  TlsMutators::SetIsDTLS();
}

#define ImportFD DTLS_ImportFD
#else
#define ImportFD SSL_ImportFD

static PRUint8 gEchConfigs[1024];
static unsigned int gEchConfigsLen;

const HpkeSymmetricSuite kEchHpkeCipherSuites[] = {
    {HpkeKdfHkdfSha256, HpkeAeadAes128Gcm},
    {HpkeKdfHkdfSha256, HpkeAeadAes256Gcm},
    {HpkeKdfHkdfSha256, HpkeAeadChaCha20Poly1305},

    {HpkeKdfHkdfSha384, HpkeAeadAes128Gcm},
    {HpkeKdfHkdfSha384, HpkeAeadAes256Gcm},
    {HpkeKdfHkdfSha384, HpkeAeadChaCha20Poly1305},

    {HpkeKdfHkdfSha512, HpkeAeadAes128Gcm},
    {HpkeKdfHkdfSha512, HpkeAeadAes256Gcm},
    {HpkeKdfHkdfSha512, HpkeAeadChaCha20Poly1305},
};
#endif  // IS_DTLS_FUZZ
const SSLCertificateCompressionAlgorithm kCompressionAlg = {
    0x1337, "fuzz", DummyCompressionEncode, DummyCompressionDecode};
const PRUint8 kPskIdentity[] = "fuzz-identity";

static void SetSocketOptions(PRFileDesc* fd,
                             std::unique_ptr<ClientConfig>& config) {
  SECStatus rv = SSL_OptionSet(fd, SSL_NO_CACHE, config->NoCache());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_EXTENDED_MASTER_SECRET,
                     config->EnableExtendedMasterSecret());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_REQUIRE_DH_NAMED_GROUPS,
                     config->RequireDhNamedGroups());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_FALSE_START, config->EnableFalseStart());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_DEFLATE, config->EnableDeflate());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_CBC_RANDOM_IV, config->EnableCbcRandomIv());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_REQUIRE_SAFE_NEGOTIATION,
                     config->RequireSafeNegotiation());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_GREASE, config->EnableGrease());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_CH_EXTENSION_PERMUTATION,
                     config->EnableCHExtensionPermutation());
  assert(rv == SECSuccess);

  if (config->SetCertificateCompressionAlgorithm()) {
    rv = SSL_SetCertificateCompressionAlgorithm(fd, kCompressionAlg);
    assert(rv == SECSuccess);
  }

  if (config->SetVersionRange()) {
    rv = SSL_VersionRangeSet(fd, &config->VersionRange());
    assert(rv == SECSuccess);
  }

  if (config->AddExternalPsk()) {
    PK11SlotInfo* slot = PK11_GetInternalSlot();
    assert(slot);

    PK11SymKey* key =
        PK11_KeyGen(slot, CKM_NSS_CHACHA20_POLY1305, nullptr, 32, nullptr);
    assert(key);

    rv = SSL_AddExternalPsk(fd, key, kPskIdentity, sizeof(kPskIdentity) - 1,
                            config->PskHashType());
    assert(rv == SECSuccess);

    PK11_FreeSlot(slot);
    PK11_FreeSymKey(key);
  }

  rv = SSL_OptionSet(fd, SSL_ENABLE_POST_HANDSHAKE_AUTH,
                     config->EnablePostHandshakeAuth());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_0RTT_DATA, config->EnableZeroRtt());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_ALPN, config->EnableAlpn());
  assert(rv == SECSuccess);

  rv =
      SSL_OptionSet(fd, SSL_ENABLE_FALLBACK_SCSV, config->EnableFallbackScsv());
  assert(rv == SECSuccess);

  rv =
      SSL_OptionSet(fd, SSL_ENABLE_OCSP_STAPLING, config->EnableOcspStapling());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_SESSION_TICKETS,
                     config->EnableSessionTickets());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_ENABLE_TLS13_COMPAT_MODE,
                     config->EnableTls13CompatMode());
  assert(rv == SECSuccess);

  rv = SSL_OptionSet(fd, SSL_NO_LOCKS, config->NoLocks());
  assert(rv == SECSuccess);

#ifndef IS_DTLS_FUZZ
  rv =
      SSL_OptionSet(fd, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_UNRESTRICTED);
  assert(rv == SECSuccess);

  if (config->SetClientEchConfigs()) {
    const sslNamedGroupDef* group_def =
        ssl_LookupNamedGroup(ssl_grp_ec_curve25519);
    assert(group_def);

    const SECOidData* oid_data = SECOID_FindOIDByTag(group_def->oidTag);
    assert(oid_data);

    ScopedSECItem params(
        SECITEM_AllocItem(nullptr, nullptr, (2 + oid_data->oid.len)));
    assert(params);

    params->data[0] = SEC_ASN1_OBJECT_ID;
    params->data[1] = oid_data->oid.len;
    PORT_Memcpy((void*)(params->data + 2), oid_data->oid.data,
                oid_data->oid.len);

    SECKEYPublicKey* pub_key = nullptr;
    SECKEYPrivateKey* priv_key =
        SECKEY_CreateECPrivateKey(params.get(), &pub_key, nullptr);
    assert(pub_key);
    assert(priv_key);

    rv = SSL_EncodeEchConfigId(
        77, "fuzz.name", 100, HpkeDhKemX25519Sha256, pub_key,
        kEchHpkeCipherSuites,
        sizeof(kEchHpkeCipherSuites) / sizeof(HpkeSymmetricSuite), gEchConfigs,
        &gEchConfigsLen, sizeof(gEchConfigs));

    SECKEY_DestroyPublicKey(pub_key);
    SECKEY_DestroyPrivateKey(priv_key);

    assert(rv == SECSuccess);

    rv = SSL_SetClientEchConfigs(fd, gEchConfigs, gEchConfigsLen);
    assert(rv == SECSuccess);
  }
#endif  // IS_DTLS_FUZZ
}

// This is only called when we set SSL_ENABLE_FALSE_START=1,
// so we can always just set *canFalseStart=true.
static SECStatus CanFalseStartCallback(PRFileDesc* fd, void* arg,
                                       PRBool* canFalseStart) {
  *canFalseStart = true;
  return SECSuccess;
}

static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checksig,
                                     PRBool isServer) {
  assert(!isServer);

  auto config = reinterpret_cast<ClientConfig*>(arg);
  if (config->FailCertificateAuthentication()) return SECFailure;

  return SECSuccess;
}

static void SetupCallbacks(PRFileDesc* fd,
                           std::unique_ptr<ClientConfig>& config) {
  SECStatus rv = SSL_AuthCertificateHook(fd, AuthCertificateHook, config.get());
  assert(rv == SECSuccess);

  rv = SSL_SetCanFalseStartCallback(fd, CanFalseStartCallback, nullptr);
  assert(rv == SECSuccess);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t len) {
  static std::unique_ptr<NSSDatabase> db(new NSSDatabase());

  EnableAllProtocolVersions();
  std::unique_ptr<ClientConfig> config(new ClientConfig(data, len));

  // Reset the RNG state.
  assert(RNG_RandomUpdate(NULL, 0) == SECSuccess);

  // Create and import dummy socket.
  std::unique_ptr<DummyPrSocket> socket(new DummyPrSocket(data, len));
  static PRDescIdentity id = PR_GetUniqueIdentity("fuzz-client");
  ScopedPRFileDesc fd(DummyIOLayerMethods::CreateFD(id, socket.get()));
  PRFileDesc* ssl_fd = ImportFD(nullptr, fd.get());
  assert(ssl_fd == fd.get());

  // Probably not too important for clients.
  SSL_SetURL(ssl_fd, "server");

  FixTime(ssl_fd);
  SetSocketOptions(ssl_fd, config);
  EnableAllCipherSuites(ssl_fd);
  SetupCallbacks(ssl_fd, config);
  DoHandshake(ssl_fd, false);

  // Release all SIDs.
  SSL_ClearSessionCache();

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size,
                                          size_t max_size, unsigned int seed) {
  Mutators mutators = {TlsMutators::DropRecord, TlsMutators::ShuffleRecords,
                       TlsMutators::DuplicateRecord,
                       TlsMutators::TruncateRecord,
                       TlsMutators::FragmentRecord};
  return CustomMutate(mutators, data, size, max_size, seed);
}

extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t* data1, size_t size1,
                                            const uint8_t* data2, size_t size2,
                                            uint8_t* out, size_t max_out_size,
                                            unsigned int seed) {
  return TlsMutators::CrossOver(data1, size1, data2, size2, out, max_out_size,
                                seed);
}

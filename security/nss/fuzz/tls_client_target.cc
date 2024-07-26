/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <memory>

#include "blapi.h"
#include "shared.h"
#include "ssl.h"
#include "sslimpl.h"
#include "tls_client_config.h"
#include "tls_common.h"
#include "tls_mutators.h"
#include "tls_socket.h"

#ifdef IS_DTLS_FUZZ
__attribute__((constructor)) static void set_is_dtls() {
  TlsMutators::SetIsDTLS();
}
#endif

const PRUint8 kDummyPskIdentity[] = "fuzz-identity";

#ifndef IS_DTLS_FUZZ
static const HpkeSymmetricSuite kDummyEchHpkeSuites[] = {
    {HpkeKdfHkdfSha256, HpkeAeadChaCha20Poly1305},
    {HpkeKdfHkdfSha256, HpkeAeadAes128Gcm}};

static PRUint8 DummyEchConfigs[1000];
static unsigned int DummyEchConfigsLen;

static SECStatus InitializeDummyEchConfigs() {
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
  PORT_Memcpy((void*)(params->data + 2), oid_data->oid.data, oid_data->oid.len);

  SECKEYPublicKey* pub_key = nullptr;
  SECKEYPrivateKey* priv_key =
      SECKEY_CreateECPrivateKey(params.get(), &pub_key, nullptr);
  assert(pub_key);
  assert(priv_key);

  SECStatus rv = SSL_EncodeEchConfigId(
      77, "fuzz.name", 100, HpkeDhKemX25519Sha256, pub_key, kDummyEchHpkeSuites,
      sizeof(kDummyEchHpkeSuites) / sizeof(HpkeSymmetricSuite), DummyEchConfigs,
      &DummyEchConfigsLen, sizeof(DummyEchConfigs));

  SECKEY_DestroyPublicKey(pub_key);
  SECKEY_DestroyPrivateKey(priv_key);

  return rv;
}
#endif  // IS_DTLS_FUZZ

PRFileDesc* ImportFD(PRFileDesc* model, PRFileDesc* fd) {
#ifdef IS_DTLS_FUZZ
  return DTLS_ImportFD(model, fd);
#else
  return SSL_ImportFD(model, fd);
#endif
}

static SECStatus DummyCompressionEncode(const SECItem* input, SECItem* output) {
  SECITEM_CopyItem(NULL, output, input);
  PORT_Memcpy(output->data, input->data, output->len);

  return SECSuccess;
}

static SECStatus DummyCompressionDecode(const SECItem* input,
                                        unsigned char* output, size_t outputLen,
                                        size_t* usedLen) {
  assert(input->len == outputLen);
  PORT_Memcpy(output, input->data, input->len);
  *usedLen = outputLen;

  return SECSuccess;
}

static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checksig,
                                     PRBool isServer) {
  assert(!isServer);

  auto config = reinterpret_cast<ClientConfig*>(arg);
  if (config->FailCertificateAuthentication()) return SECFailure;

  return SECSuccess;
}

static void SetSocketOptions(PRFileDesc* fd,
                             std::unique_ptr<ClientConfig>& config) {
  SECStatus rv = SSL_OptionSet(fd, SSL_NO_CACHE, config->EnableCache());
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

#ifndef IS_DTLS_FUZZ
  rv =
      SSL_OptionSet(fd, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_UNRESTRICTED);
  assert(rv == SECSuccess);
#endif

  if (config->SetCertificateCompressionAlgorithm()) {
    rv = SSLExp_SetCertificateCompressionAlgorithm(
        fd, {
                .id = 0x1337,
                .name = "fuzz-compression",
                .encode = DummyCompressionEncode,
                .decode = DummyCompressionDecode,
            });
    assert(rv == SECSuccess);
  }

#ifndef IS_DTLS_FUZZ
  if (config->SetClientEchConfigs()) {
    rv = InitializeDummyEchConfigs();
    assert(rv == SECSuccess);

    rv = SSL_SetClientEchConfigs(fd, DummyEchConfigs, DummyEchConfigsLen);
    assert(rv == SECSuccess);
  }
#endif

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

    rv = SSL_AddExternalPsk(fd, key, kDummyPskIdentity, sizeof(kDummyPskIdentity) - 1,
                            ssl_hash_sha256);
    assert(rv == SECSuccess);

    PK11_FreeSlot(slot);
    PK11_FreeSymKey(key);
  }

  rv = SSL_OptionSet(fd, SSL_ENABLE_POST_HANDSHAKE_AUTH,
                     config->EnablePostHandshakeAuth());
  assert(rv == SECSuccess);
}

// This is only called when we set SSL_ENABLE_FALSE_START=1,
// so we can always just set *canFalseStart=true.
static SECStatus CanFalseStartCallback(PRFileDesc* fd, void* arg,
                                       PRBool* canFalseStart) {
  *canFalseStart = true;
  return SECSuccess;
}

static void SetupCallbacks(PRFileDesc* fd, ClientConfig* config) {
  SECStatus rv = SSL_AuthCertificateHook(fd, AuthCertificateHook, config);
  assert(rv == SECSuccess);

  rv = SSL_SetCanFalseStartCallback(fd, CanFalseStartCallback, nullptr);
  assert(rv == SECSuccess);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t len) {
  std::unique_ptr<NSSDatabase> db(new NSSDatabase());
  assert(db != nullptr);

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
  SetupCallbacks(ssl_fd, config.get());
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

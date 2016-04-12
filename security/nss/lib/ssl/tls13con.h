/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __tls13con_h_
#define __tls13con_h_

typedef enum {
    StaticSharedSecret,
    EphemeralSharedSecret
} SharedSecretType;

SECStatus tls13_UnprotectRecord(
    sslSocket *ss, SSL3Ciphertext *cText, sslBuffer *plaintext,
    SSL3AlertDescription *alert);
unsigned char *
tls13_EncodeUintX(PRUint32 value, unsigned int bytes, unsigned char *to);

#if defined(WIN32)
#define __func__ __FUNCTION__
#endif

void tls13_SetHsState(sslSocket *ss, SSL3WaitState ws,
                      const char *func, const char *file, int line);
#define TLS13_SET_HS_STATE(ss, ws) \
    tls13_SetHsState(ss, ws, __func__, __FILE__, __LINE__)

/* Return PR_TRUE if the socket is in one of the given states, else return
 * PR_FALSE. Only call the macro not the function, because the trailing
 * wait_invalid is needed to terminate the argument list. */
PRBool tls13_InHsState(sslSocket *ss, ...);
#define TLS13_IN_HS_STATE(ss, ...) \
    tls13_InHsState(ss, __VA_ARGS__, wait_invalid)

SSLHashType tls13_GetHash(sslSocket *ss);
CK_MECHANISM_TYPE tls13_GetHkdfMechanism(sslSocket *ss);
void tls13_FatalError(sslSocket *ss, PRErrorCode prError,
                      SSL3AlertDescription desc);
SECStatus tls13_SetupClientHello(sslSocket *ss);
PRBool tls13_AllowPskCipher(const sslSocket *ss,
                            const ssl3CipherSuiteDef *cipher_def);
SECStatus tls13_HandleClientHelloPart2(sslSocket *ss,
                                       const SECItem *suites,
                                       sslSessionID *sid);
SECStatus tls13_HandleServerHelloPart2(sslSocket *ss);
SECStatus tls13_HandlePostHelloHandshakeMessage(sslSocket *ss, SSL3Opaque *b,
                                                PRUint32 length,
                                                SSL3Hashes *hashesPtr);
SECStatus tls13_HandleClientKeyShare(sslSocket *ss);
SECStatus tls13_SendServerHelloSequence(sslSocket *ss);
SECStatus tls13_HandleServerKeyShare(sslSocket *ss);
SECStatus tls13_AddContextToHashes(sslSocket *ss,
                                   SSL3Hashes *hashes /* IN/OUT */,
                                   SSLHashType algorithm, PRBool sending);
void tls13_DestroyKeyShareEntry(TLS13KeyShareEntry *entry);
void tls13_DestroyKeyShares(PRCList *list);
PRBool tls13_ExtensionAllowed(PRUint16 extension, SSL3HandshakeType message);
SECStatus tls13_ProtectRecord(sslSocket *ss,
                              ssl3CipherSpec *cwSpec,
                              SSL3ContentType type,
                              const SSL3Opaque *pIn,
                              PRUint32 contentLen,
                              sslBuffer *wrBuf);

#endif /* __tls13con_h_ */

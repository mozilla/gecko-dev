/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PK11SDR_H_
#define _PK11SDR_H_

#include "pkcs11t.h"
#include "seccomon.h"
#include "secmodt.h"

SEC_BEGIN_PROTOS

/*
 * PK11SDR_Encrypt
 *  Deprecated version of PK11SDR_EncryptWithMechanism using DES3_CBC.
 */
SECStatus
PK11SDR_Encrypt(SECItem *keyid, SECItem *data, SECItem *result, void *cx);

/*
 * PK11SDR_EncryptWithMechanism
 *  Encrypt a block of data using the symmetric key identified and the
 *  encryption mechanism specified (only AES_CBC and DES3_CBC are supported).
 *  The result is an ASN.1 (DER) encoded block of keyid, params and data.
 */
SECStatus
PK11SDR_EncryptWithMechanism(PK11SlotInfo *slot, SECItem *keyid, CK_MECHANISM_TYPE type, SECItem *data, SECItem *result, void *cx);

/*
 * PK11SDR_Decrypt - decrypt data previously encrypted with PK11SDR_Encrypt
 * result should be freed with SECItem_ZfreeItem
 */
SECStatus
PK11SDR_Decrypt(SECItem *data, SECItem *result, void *cx);

SEC_END_PROTOS

#endif

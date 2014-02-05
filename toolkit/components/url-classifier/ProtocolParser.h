//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProtocolParser_h__
#define ProtocolParser_h__

#include "HashStore.h"
#include "nsICryptoHMAC.h"

namespace mozilla {
namespace safebrowsing {

/**
 * Some helpers for parsing the safe
 */
class ProtocolParser {
public:
  struct ForwardedUpdate {
    nsCString table;
    nsCString url;
  };

  ProtocolParser();
  ~ProtocolParser();

  nsresult Status() const { return mUpdateStatus; }

  nsresult Init(nsICryptoHash* aHasher);

  void SetCurrentTable(const nsACString& aTable);

  nsresult Begin();
  nsresult AppendStream(const nsACString& aData);

  // Forget the table updates that were created by this pass.  It
  // becomes the caller's responsibility to free them.  This is shitty.
  TableUpdate *GetTableUpdate(const nsACString& aTable);
  void ForgetTableUpdates() { mTableUpdates.Clear(); }
  nsTArray<TableUpdate*> &GetTableUpdates() { return mTableUpdates; }

  // Update information.
  const nsTArray<ForwardedUpdate> &Forwards() const { return mForwards; }
  int32_t UpdateWait() { return mUpdateWait; }
  bool ResetRequested() { return mResetRequested; }

private:
  nsresult ProcessControl(bool* aDone);
  nsresult ProcessExpirations(const nsCString& aLine);
  nsresult ProcessChunkControl(const nsCString& aLine);
  nsresult ProcessForward(const nsCString& aLine);
  nsresult AddForward(const nsACString& aUrl);
  nsresult ProcessChunk(bool* done);
  // Remove this, it's only used for testing
  nsresult ProcessPlaintextChunk(const nsACString& aChunk);
  nsresult ProcessShaChunk(const nsACString& aChunk);
  nsresult ProcessHostAdd(const Prefix& aDomain, uint8_t aNumEntries,
                          const nsACString& aChunk, uint32_t* aStart);
  nsresult ProcessHostSub(const Prefix& aDomain, uint8_t aNumEntries,
                          const nsACString& aChunk, uint32_t* aStart);
  nsresult ProcessHostAddComplete(uint8_t aNumEntries, const nsACString& aChunk,
                                  uint32_t *aStart);
  nsresult ProcessHostSubComplete(uint8_t numEntries, const nsACString& aChunk,
                                  uint32_t* start);
  // Digest chunks are very similar to shavar chunks, except digest chunks
  // always contain the full hash, so there is no need for chunk data to
  // contain prefix sizes.
  nsresult ProcessDigestChunk(const nsACString& aChunk);
  nsresult ProcessDigestAdd(const nsACString& aChunk);
  nsresult ProcessDigestSub(const nsACString& aChunk);
  bool NextLine(nsACString& aLine);

  void CleanupUpdates();

  enum ParserState {
    PROTOCOL_STATE_CONTROL,
    PROTOCOL_STATE_CHUNK
  };
  ParserState mState;

  enum ChunkType {
    // Types for shavar tables.
    CHUNK_ADD,
    CHUNK_SUB,
    // Types for digest256 tables. digest256 tables differ in format from
    // shavar tables since they only contain complete hashes.
    CHUNK_ADD_DIGEST,
    CHUNK_SUB_DIGEST
  };

  struct ChunkState {
    ChunkType type;
    uint32_t num;
    uint32_t hashSize;
    uint32_t length;
    void Clear() { num = 0; hashSize = 0; length = 0; }
  };
  ChunkState mChunkState;

  nsCOMPtr<nsICryptoHash> mCryptoHash;

  nsresult mUpdateStatus;
  nsCString mPending;

  uint32_t mUpdateWait;
  bool mResetRequested;

  nsTArray<ForwardedUpdate> mForwards;
  // Keep track of updates to apply before passing them to the DBServiceWorkers.
  nsTArray<TableUpdate*> mTableUpdates;
  // Updates to apply to the current table being parsed.
  TableUpdate *mTableUpdate;
};

}
}

#endif

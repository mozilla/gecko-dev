/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum MLSObjectType {
  "group-epoch",
  "group-identifier",
  "group-info",
  "client-identifier",
  "credential-basic",
  "key-package",
  "proposal",
  "commit-output",
  "commit-processed",
  "welcome",
  "exporter-output",
  "exporter-label",
  "exporter-context",
  "application-message-ciphertext",
  "application-message-plaintext",
};

dictionary MLSBytes {
  required MLSObjectType type;
  required Uint8Array content;
};

dictionary MLSGroupMember {
  required Uint8Array clientId;
  required Uint8Array credential;
};

dictionary MLSGroupDetails {
  required MLSObjectType type;
  required Uint8Array groupId;
  required Uint8Array groupEpoch;
  required sequence<MLSGroupMember> members;
};

dictionary MLSCommitOutput {
  required MLSObjectType type;
  required Uint8Array groupId;
  required Uint8Array commit;
  Uint8Array welcome;
  Uint8Array groupInfo;
  Uint8Array ratchetTree;
  Uint8Array clientId;
};

dictionary MLSExporterOutput {
  required MLSObjectType type;
  required Uint8Array groupId;
  required Uint8Array groupEpoch;
  required Uint8Array label;
  required Uint8Array context;
  required Uint8Array secret;
};

dictionary MLSReceived {
  required MLSObjectType type;
  required Uint8Array groupId;
  Uint8Array groupEpoch;
  Uint8Array content;
  MLSCommitOutput commitOutput;
};

typedef MLSBytes MLSClientId;
typedef MLSBytes MLSGroupId;
typedef MLSBytes MLSCredential;
typedef MLSBytes MLSKeyPackage;
typedef MLSBytes MLSProposal;
typedef (MLSBytes or Uint8Array) MLSBytesOrUint8Array;
typedef (Uint8Array or UTF8String) Uint8ArrayOrUTF8String;
typedef (MLSBytes or Uint8Array or UTF8String) MLSBytesOrUint8ArrayOrUTF8String;

[Pref="security.mls.enabled",
  SecureContext,
  Exposed=(Window,Worker)]
interface MLS {
  [Throws]
  constructor();
  [Throws]
  Promise<undefined> deleteState();
  [Throws]
  Promise<MLSClientId> generateIdentity();
  [Throws]
  Promise<MLSCredential> generateCredential(MLSBytesOrUint8ArrayOrUTF8String credentialContent);
  [Throws]
  Promise<MLSKeyPackage> generateKeyPackage(MLSBytesOrUint8Array clientId, MLSBytesOrUint8Array credential);
  [Throws]
  Promise<MLSGroupView> groupCreate(MLSBytesOrUint8Array clientId, MLSBytesOrUint8Array credential);
  [Throws]
  Promise<MLSGroupView?> groupGet(MLSBytesOrUint8Array groupId, MLSBytesOrUint8Array clientId);
  [Throws]
  Promise<MLSGroupView> groupJoin(MLSBytesOrUint8Array clientId, MLSBytesOrUint8Array welcome);
  // Utility functions
  [Throws]
  Promise<MLSGroupId> getGroupIdFromMessage(MLSBytesOrUint8Array message);
};

[Pref="security.mls.enabled",
  SecureContext,
  Exposed=(Window,Worker)]
interface MLSGroupView {
  [Throws]
  readonly attribute Uint8Array groupId;
  [Throws]
  readonly attribute Uint8Array clientId;
  [Throws]
  Promise<undefined> deleteState();
  [Throws]
  Promise<MLSCommitOutput> add(MLSBytesOrUint8Array keyPackage);
  [Throws]
  Promise<MLSProposal> proposeAdd(MLSBytesOrUint8Array keyPackage);
  [Throws]
  Promise<MLSCommitOutput> remove(MLSBytesOrUint8Array remClientId);
  [Throws]
  Promise<MLSProposal> proposeRemove(MLSBytesOrUint8Array remClientId);
  [Throws]
  Promise<MLSCommitOutput> close();
  [Throws]
  Promise<MLSGroupDetails> details();
  [Throws]
  Promise<MLSBytes> send(MLSBytesOrUint8ArrayOrUTF8String message);
  [Throws]
  Promise<MLSReceived> receive(MLSBytesOrUint8Array message);
  [Throws]
  Promise<MLSReceived> applyPendingCommit();
  [Throws]
  Promise<MLSExporterOutput> exportSecret(MLSBytesOrUint8ArrayOrUTF8String label, MLSBytesOrUint8Array context, unsigned long long length);
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;

import org.json.simple.JSONArray;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

public class ClientRecord extends Record {
  private static final String LOG_TAG = "ClientRecord";

  public static final String CLIENT_TYPE         = "mobile";
  public static final String COLLECTION_NAME     = "clients";
  public static final long CLIENTS_TTL = 21 * 24 * 60 * 60; // 21 days in seconds.
  public static final String DEFAULT_CLIENT_NAME = "Default Name";

  public static final String PROTOCOL_LEGACY_SYNC = "1.1";
  public static final String PROTOCOL_FXA_SYNC = "1.5";

  /**
   * Each of these fields is 'owned' by the client it represents. For example,
   * the "version" field is the Firefox version of that client; some time after
   * that client upgrades, it'll upload a new record with its new version.
   *
   * The only exception is for commands. When a command is sent to a client, the
   * sender will download its current record, append the command to the
   * "commands" array, and reupload the record. After processing, the recipient
   * will reupload its record with an empty commands array.
   *
   * Note that the version, then, will remain the version of the recipient, as
   * with the other descriptive fields.
   */
  public String name = ClientRecord.DEFAULT_CLIENT_NAME;
  public String type = ClientRecord.CLIENT_TYPE;
  public String version = null;                      // Free-form string, optional.
  public JSONArray commands;
  public JSONArray protocols;

  public ClientRecord(String guid, String collection, long lastModified, boolean deleted) {
    super(guid, collection, lastModified, deleted);
    this.ttl = CLIENTS_TTL;
  }

  public ClientRecord(String guid, String collection, long lastModified) {
    this(guid, collection, lastModified, false);
  }

  public ClientRecord(String guid, String collection) {
    this(guid, collection, 0, false);
  }

  public ClientRecord(String guid) {
    this(guid, COLLECTION_NAME, 0, false);
  }

  public ClientRecord() {
    this(Utils.generateGuid(), COLLECTION_NAME, 0, false);
  }

  @Override
  protected void initFromPayload(ExtendedJSONObject payload) {
    this.name = (String) payload.get("name");
    this.type = (String) payload.get("type");
    try {
      this.version = (String) payload.get("version");
    } catch (Exception e) {
      // Oh well.
    }

    try {
      commands = payload.getArray("commands");
    } catch (NonArrayJSONException e) {
      Logger.debug(LOG_TAG, "Got non-array commands in client record " + guid, e);
      commands = null;
    }

    try {
      protocols = payload.getArray("protocols");
    } catch (NonArrayJSONException e) {
      Logger.debug(LOG_TAG, "Got non-array protocols in client record " + guid, e);
      protocols = null;
    }
  }

  @Override
  protected void populatePayload(ExtendedJSONObject payload) {
    putPayload(payload, "id",   this.guid);
    putPayload(payload, "name", this.name);
    putPayload(payload, "type", this.type);
    putPayload(payload, "version", this.version);

    if (this.commands != null) {
      payload.put("commands",  this.commands);
    }

    if (this.protocols != null) {
      payload.put("protocols",  this.protocols);
    }
  }

  @Override
  public boolean equals(Object o) {
    if (!(o instanceof ClientRecord) || !super.equals(o)) {
      return false;
    }

    return this.equalPayloads(o);
  }

  @Override
  public boolean equalPayloads(Object o) {
    if (!(o instanceof ClientRecord) || !super.equalPayloads(o)) {
      return false;
    }

    // Don't compare versions or protocols, no matter how much we might want to.
    // They're not required by the spec.
    ClientRecord other = (ClientRecord) o;
    if (!RepoUtils.stringsEqual(other.name, this.name) ||
        !RepoUtils.stringsEqual(other.type, this.type)) {
      return false;
    }
    return true;
  }

  @Override
  public Record copyWithIDs(String guid, long androidID) {
    ClientRecord out = new ClientRecord(guid, this.collection, this.lastModified, this.deleted);
    out.androidID = androidID;
    out.sortIndex = this.sortIndex;
    out.ttl       = this.ttl;

    out.name = this.name;
    out.type = this.type;
    out.version = this.version;
    out.protocols = this.protocols;
    return out;
  }

/*
Example record:

{id:"relf31w7B4F1",
 name:"marina_mac",
 type:"mobile"
 commands:[{"args":["bookmarks"],"command":"wipeEngine"},
           {"args":["forms"],"command":"wipeEngine"},
           {"args":["history"],"command":"wipeEngine"},
           {"args":["passwords"],"command":"wipeEngine"},
           {"args":["prefs"],"command":"wipeEngine"},
           {"args":["tabs"],"command":"wipeEngine"},
           {"args":["addons"],"command":"wipeEngine"}]}
*/
}

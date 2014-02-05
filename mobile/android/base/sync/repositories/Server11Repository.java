/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;

/**
 * A Server11Repository implements fetching and storing against the Sync 1.1 API.
 * It doesn't do crypto: that's the job of the middleware.
 *
 * @author rnewman
 */
public class Server11Repository extends Repository {
  protected String collection;
  protected URI collectionURI;
  protected final AuthHeaderProvider authHeaderProvider;

  /**
   * Construct a new repository that fetches and stores against the Sync 1.1. API.
   *
   * @param collection name.
   * @param storageURL full URL to storage endpoint.
   * @param authHeaderProvider to use in requests.
   * @throws URISyntaxException
   */
  public Server11Repository(String collection, String storageURL, AuthHeaderProvider authHeaderProvider) throws URISyntaxException {
    this.collection = collection;
    this.collectionURI = new URI(storageURL + (storageURL.endsWith("/") ? collection : "/" + collection));
    this.authHeaderProvider = authHeaderProvider;
  }

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate,
                            Context context) {
    delegate.onSessionCreated(new Server11RepositorySession(this));
  }

  public URI collectionURI() {
    return this.collectionURI;
  }

  public URI collectionURI(boolean full, long newer, long limit, String sort, String ids) throws URISyntaxException {
    ArrayList<String> params = new ArrayList<String>();
    if (full) {
      params.add("full=1");
    }
    if (newer >= 0) {
      // Translate local millisecond timestamps into server decimal seconds.
      String newerString = Utils.millisecondsToDecimalSecondsString(newer);
      params.add("newer=" + newerString);
    }
    if (limit > 0) {
      params.add("limit=" + limit);
    }
    if (sort != null) {
      params.add("sort=" + sort);       // We trust these values.
    }
    if (ids != null) {
      params.add("ids=" + ids);         // We trust these values.
    }

    if (params.size() == 0) {
      return this.collectionURI;
    }

    StringBuilder out = new StringBuilder();
    char indicator = '?';
    for (String param : params) {
      out.append(indicator);
      indicator = '&';
      out.append(param);
    }
    String uri = this.collectionURI + out.toString();
    return new URI(uri);
  }

  public URI wboURI(String id) throws URISyntaxException {
    return new URI(this.collectionURI + "/" + id);
  }

  // Override these.
  @SuppressWarnings("static-method")
  protected long getDefaultFetchLimit() {
    return -1;
  }

  @SuppressWarnings("static-method")
  protected String getDefaultSort() {
    return null;
  }

  public AuthHeaderProvider getAuthHeaderProvider() {
    return authHeaderProvider;
  }
}

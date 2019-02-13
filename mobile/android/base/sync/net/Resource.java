/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.net.URI;

import ch.boye.httpclientandroidlib.HttpEntity;

public interface Resource {
  public abstract URI getURI();
  public abstract String getURIString();
  public abstract String getHostname();
  public abstract void get();
  public abstract void delete();
  public abstract void post(HttpEntity body);
  public abstract void patch(HttpEntity body);
  public abstract void put(HttpEntity body);
}

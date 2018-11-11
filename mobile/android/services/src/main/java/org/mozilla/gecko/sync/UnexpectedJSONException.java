/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

public class UnexpectedJSONException extends Exception {
  private static final long serialVersionUID = 4797570033096443169L;

  public UnexpectedJSONException(String detailMessage) {
    super(detailMessage);
  }

  public UnexpectedJSONException(Throwable throwable) {
    super(throwable);
  }

  public static class BadRequiredFieldJSONException extends UnexpectedJSONException {
    private static final long serialVersionUID = -9207736984784497612L;

    public BadRequiredFieldJSONException(String string) {
      super(string);
    }
  }
}
